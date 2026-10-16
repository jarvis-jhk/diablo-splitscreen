#include "localcoop.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include "utils/format.hpp"

#include "controls/axis_direction.h"
#include "controls/controller.h"
#include "controls/controller_buttons.h"
#ifndef USE_SDL1
#include "controls/devices/game_controller.h"
#endif
#include "controls/devices/joystick.h"
#include "control/control.hpp"
#include "cursor.h"
#include "engine/point.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "engine/surface.hpp"
#include "game_mode.hpp"
#include "inv.h"
#include "items.h"
#include "levels/dun_tile_data.hpp"
#include "levels/gendung.h"
#include "levels/tile_properties.hpp"
#include "lighting.h"
#include "menu.h"
#include "monster.h"
#include "msg.h"
#include "multi.h"
#include "pfile.h"
#include "player.h"
#include "plrmsg.h"
#include "spells.h"
#include "tables/spelldat.h"
#include "utils/endian_swap.hpp"
#include "utils/language.h"
#include "utils/str_cat.hpp"
#include "utils/utf8.hpp"

namespace devilution {

namespace {

// How far (in tiles) a guest may walk away from MyPlayer: the camera still
// follows pad 1, so this keeps every hero on the shared screen.
constexpr int LeashDistance = 7;
constexpr float StickDeadzone = 0.35F;

struct Guest {
	uint8_t playerId;
	SDL_JoystickID device;
	std::optional<uint32_t> saveNumber;
	SpellID spell = SpellID::Invalid;
	bool attackHeld = false;
	bool walking = false;
	// The guest's own inventory panel: its slot cursor (inv_xy_slot) and the
	// stick direction it is being steered with, for key-repeat.
	bool inventoryOpen = false;
	int slot = SLOTXY_INV_FIRST;
	AxisDirection navDirection { AxisDirectionX_NONE, AxisDirectionY_NONE };
	int navTicks = 0;
};

std::vector<Guest> Guests;
SDL_JoystickID PrimaryDevice = -1;

struct PendingCommand {
	uint8_t playerId;
	std::vector<std::byte> data;
};
std::vector<PendingCommand> PendingCommands;

Guest *FindGuest(SDL_JoystickID device)
{
	for (Guest &guest : Guests) {
		if (guest.device == device)
			return &guest;
	}
	return nullptr;
}

template <typename T>
void Send(uint8_t playerId, const T &cmd)
{
	SendGuestCommand(playerId, reinterpret_cast<const std::byte *>(&cmd), sizeof(cmd));
}

void SendLoc(uint8_t playerId, _cmd_id id, Point position)
{
	TCmdLoc cmd;
	cmd.bCmd = id;
	cmd.x = static_cast<uint8_t>(position.x);
	cmd.y = static_cast<uint8_t>(position.y);
	Send(playerId, cmd);
}

void SendParam1(uint8_t playerId, _cmd_id id, uint16_t param)
{
	TCmdParam1 cmd;
	cmd.bCmd = id;
	cmd.wParam1 = Swap16LE(param);
	Send(playerId, cmd);
}

bool IsDeviceConnected(SDL_JoystickID device)
{
#ifndef USE_SDL1
	if (GameController::Get(device) != nullptr)
		return true;
#endif
	return Joystick::Get(device) != nullptr;
}

float NormalizedAxis(int16_t value)
{
	return std::clamp(static_cast<float>(value) / 32767.F, -1.F, 1.F);
}

// The guest's own left stick, read from its device only.
void ReadLeftStick(SDL_JoystickID device, float &x, float &y)
{
	x = 0;
	y = 0;
#ifdef USE_SDL3
	if (SDL_Gamepad *gamepad = SDL_GetGamepadFromID(device); gamepad != nullptr) {
		x = NormalizedAxis(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX));
		y = NormalizedAxis(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY));
		return;
	}
	if (SDL_Joystick *joystick = SDL_GetJoystickFromID(device); joystick != nullptr && SDL_GetNumJoystickAxes(joystick) >= 2) {
		x = NormalizedAxis(SDL_GetJoystickAxis(joystick, 0));
		y = NormalizedAxis(SDL_GetJoystickAxis(joystick, 1));
	}
#elif !defined(USE_SDL1)
	if (SDL_GameController *gamepad = SDL_GameControllerFromInstanceID(device); gamepad != nullptr) {
		x = NormalizedAxis(SDL_GameControllerGetAxis(gamepad, SDL_CONTROLLER_AXIS_LEFTX));
		y = NormalizedAxis(SDL_GameControllerGetAxis(gamepad, SDL_CONTROLLER_AXIS_LEFTY));
		return;
	}
	if (SDL_Joystick *joystick = SDL_JoystickFromInstanceID(device); joystick != nullptr && SDL_JoystickNumAxes(joystick) >= 2) {
		x = NormalizedAxis(SDL_JoystickGetAxis(joystick, 0));
		y = NormalizedAxis(SDL_JoystickGetAxis(joystick, 1));
	}
#else
	(void)device;
#endif
}

AxisDirection GuestDirection(const Guest &guest)
{
	AxisDirection dir { AxisDirectionX_NONE, AxisDirectionY_NONE };
	if (IsControllerButtonPressedOnDevice(guest.device, ControllerButton_BUTTON_DPAD_LEFT))
		dir.x = AxisDirectionX_LEFT;
	else if (IsControllerButtonPressedOnDevice(guest.device, ControllerButton_BUTTON_DPAD_RIGHT))
		dir.x = AxisDirectionX_RIGHT;
	if (IsControllerButtonPressedOnDevice(guest.device, ControllerButton_BUTTON_DPAD_UP))
		dir.y = AxisDirectionY_UP;
	else if (IsControllerButtonPressedOnDevice(guest.device, ControllerButton_BUTTON_DPAD_DOWN))
		dir.y = AxisDirectionY_DOWN;
	if (dir.x != AxisDirectionX_NONE || dir.y != AxisDirectionY_NONE)
		return dir;

	float x;
	float y;
	ReadLeftStick(guest.device, x, y);
	if (x * x + y * y < StickDeadzone * StickDeadzone)
		return dir;
	// SDL's stick y grows downwards; 8-way sectors at 22.5°.
	constexpr float DiagonalCutoff = 0.41421356F;
	const float absX = std::abs(x);
	const float absY = std::abs(y);
	if (absX > 0 && absY / absX < 1.F / DiagonalCutoff)
		dir.x = x > 0 ? AxisDirectionX_RIGHT : AxisDirectionX_LEFT;
	if (absX == 0 || absY / absX > DiagonalCutoff)
		dir.y = y < 0 ? AxisDirectionY_UP : AxisDirectionY_DOWN;
	return dir;
}

Direction ToDirection(AxisDirection dir)
{
	static constexpr Direction FaceDir[3][3] = {
		{ Direction::South, Direction::North, Direction::South },
		{ Direction::West, Direction::NorthWest, Direction::SouthWest },
		{ Direction::East, Direction::NorthEast, Direction::SouthEast },
	};
	return FaceDir[static_cast<size_t>(dir.x)][static_cast<size_t>(dir.y)];
}

bool CanTarget(const Monster &monster)
{
	if ((monster.flags & MFLAG_HIDDEN) != 0 || monster.isPlayerMinion() || monster.hasNoLife())
		return false;
	if (monster.mode == MonsterMode::Death)
		return false;
	return IsTileLit(monster.position.tile) && dMonster[monster.position.tile.x][monster.position.tile.y] != 0;
}

std::optional<size_t> NearestMonster(const Player &player, int maxDistance, bool needLineOfSight)
{
	std::optional<size_t> best;
	int bestDistance = maxDistance + 1;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const size_t id = ActiveMonsters[i];
		const Monster &monster = Monsters[id];
		if (!CanTarget(monster))
			continue;
		const int distance = player.position.future.WalkingDistance(monster.position.future);
		if (distance >= bestDistance)
			continue;
		if (needLineOfSight && !LineClearMissile(player.position.tile, monster.position.tile))
			continue;
		best = id;
		bestDistance = distance;
	}
	return best;
}

std::optional<Point> NearestItem(const Player &player)
{
	std::optional<Point> best;
	int bestDistance = 3;
	for (int dx = -2; dx <= 2; dx++) {
		for (int dy = -2; dy <= 2; dy++) {
			const Point position = player.position.future + Displacement { dx, dy };
			if (!InDungeonBounds(position) || dItem[position.x][position.y] <= 0)
				continue;
			const int distance = player.position.future.WalkingDistance(position);
			if (distance < bestDistance) {
				best = position;
				bestDistance = distance;
			}
		}
	}
	return best;
}

// Spells a guest can aim with its own facing or nearest target. Cursor-driven
// spells (identify, repair, telekinesis, ...) need a personal cursor, and town
// portal/golem still send as pad 1; both stay pad 1's until guests get panels.
bool IsAimableByGuest(SpellID spell)
{
	switch (spell) {
	case SpellID::Identify:
	case SpellID::TownPortal:
	case SpellID::Golem:
	case SpellID::ItemRepair:
	case SpellID::StaffRecharge:
	case SpellID::TrapDisarm:
	case SpellID::Resurrect:
	case SpellID::Telekinesis:
	case SpellID::HealOther:
	case SpellID::Search:
	case SpellID::Warp:
		return false;
	default:
		break;
	}
	return IsValidSpell(spell);
}

bool IsGuestSpell(const Player &player, SpellID spell)
{
	if (!IsAimableByGuest(spell))
		return false;
	return (player._pMemSpells & GetSpellBitmask(spell)) != 0 && player.GetSpellLevel(spell) > 0;
}

SpellID NextGuestSpell(const Player &player, SpellID current)
{
	const int first = static_cast<int>(SpellID::Firebolt);
	const int last = static_cast<int>(SpellID::LAST);
	const int count = last - first + 1;
	const int start = current == SpellID::Invalid ? first - 1 : static_cast<int>(current);
	for (int step = 1; step <= count; step++) {
		const int candidate = first + ((start - first + step) % count + count) % count;
		if (IsGuestSpell(player, static_cast<SpellID>(candidate)))
			return static_cast<SpellID>(candidate);
	}
	return SpellID::Invalid;
}

Point AimPoint(const Player &player, int distance)
{
	Point target = player.position.tile;
	for (int i = 0; i < distance; i++) {
		const Point next = target + player._pdir;
		if (!InDungeonBounds(next))
			break;
		target = next;
	}
	return target;
}

void GuestPrimaryAction(Guest &guest)
{
	Player &player = Players[guest.playerId];
	if (leveltype != DTYPE_TOWN) {
		const bool ranged = player.UsesRangedWeapon();
		if (const std::optional<size_t> monster = NearestMonster(player, ranged ? 10 : 3, ranged); monster) {
			SendParam1(guest.playerId, ranged ? CMD_RATTACKID : CMD_ATTACKID, static_cast<uint16_t>(*monster));
			return;
		}
	}
	if (const std::optional<Point> item = NearestItem(player); item) {
		TCmdLocParam1 cmd;
		cmd.bCmd = CMD_GOTOAGETITEM;
		cmd.x = static_cast<uint8_t>(item->x);
		cmd.y = static_cast<uint8_t>(item->y);
		cmd.wParam1 = Swap16LE(static_cast<uint16_t>(dItem[item->x][item->y] - 1));
		Send(guest.playerId, cmd);
		return;
	}
	if (leveltype != DTYPE_TOWN)
		SendLoc(guest.playerId, player.UsesRangedWeapon() ? CMD_RATTACKXY : CMD_SATTACKXY, AimPoint(player, player.UsesRangedWeapon() ? 5 : 1));
}

// Casts a spell (or reads a scroll: type Scroll, from its inv_item slot) at
// the nearest monster or straight ahead.
void GuestCast(Guest &guest, SpellID spell, SpellType type, int spellFrom)
{
	Player &player = Players[guest.playerId];
	if (leveltype == DTYPE_TOWN && !GetSpellData(spell).isAllowedInTown()) {
		player.Say(HeroSpeech::ICantCastThatHere);
		return;
	}
	const SpellCheckResult check = CheckSpell(player, spell, type, false);
	if (check == SpellCheckResult::Fail_NoMana) {
		player.Say(HeroSpeech::NotEnoughMana);
		return;
	}
	if (check != SpellCheckResult::Success)
		return;

	const auto spellParam = static_cast<uint16_t>(static_cast<int8_t>(spell));
	const auto typeParam = static_cast<uint16_t>(static_cast<uint8_t>(type));
	const auto fromParam = static_cast<uint16_t>(spellFrom);
	const std::optional<size_t> monster = leveltype != DTYPE_TOWN ? NearestMonster(player, 10, true) : std::nullopt;
	if (IsWallSpell(spell)) {
		const Point target = monster ? static_cast<Point>(Monsters[*monster].position.tile) : AimPoint(player, 3);
		TCmdLocParam4 cmd;
		cmd.bCmd = CMD_SPELLXYD;
		cmd.x = static_cast<uint8_t>(target.x);
		cmd.y = static_cast<uint8_t>(target.y);
		cmd.wParam1 = Swap16LE(spellParam);
		cmd.wParam2 = Swap16LE(typeParam);
		cmd.wParam3 = Swap16LE(static_cast<uint16_t>(GetDirection(player.position.tile, target)));
		cmd.wParam4 = Swap16LE(fromParam);
		Send(guest.playerId, cmd);
	} else if (monster) {
		TCmdParam4 cmd;
		cmd.bCmd = CMD_SPELLID;
		cmd.wParam1 = Swap16LE(static_cast<uint16_t>(*monster));
		cmd.wParam2 = Swap16LE(spellParam);
		cmd.wParam3 = Swap16LE(typeParam);
		cmd.wParam4 = Swap16LE(fromParam);
		Send(guest.playerId, cmd);
	} else {
		const Point target = AimPoint(player, 4);
		TCmdLocParam3 cmd;
		cmd.bCmd = CMD_SPELLXY;
		cmd.x = static_cast<uint8_t>(target.x);
		cmd.y = static_cast<uint8_t>(target.y);
		cmd.wParam1 = Swap16LE(spellParam);
		cmd.wParam2 = Swap16LE(typeParam);
		cmd.wParam3 = Swap16LE(fromParam);
		Send(guest.playerId, cmd);
	}
}

void GuestCastSpell(Guest &guest)
{
	Player &player = Players[guest.playerId];
	if (!IsGuestSpell(player, guest.spell)) {
		player.Say(HeroSpeech::IDontHaveASpellReady);
		return;
	}
	GuestCast(guest, guest.spell, SpellType::Spell, 0);
}

void GuestDrinkPotion(Guest &guest)
{
	Player &player = Players[guest.playerId];
	if (player._pInvincible || player.hasNoLife())
		return;
	const bool wantsLife = player._pHitPoints < player._pMaxHP;
	const bool wantsMana = player._pMana < player._pMaxMana;
	int best = -1;
	int bestScore = 0;
	for (int i = 0; i < MaxBeltItems; i++) {
		const Item &item = player.SpdList[i];
		if (item.isEmpty())
			continue;
		int score = 0;
		switch (item._iMiscId) {
		case IMISC_HEAL:
		case IMISC_FULLHEAL:
			score = wantsLife ? 3 : 0;
			break;
		case IMISC_MANA:
		case IMISC_FULLMANA:
			score = wantsMana ? 2 : 0;
			break;
		case IMISC_REJUV:
		case IMISC_FULLREJUV:
			score = (wantsLife || wantsMana) ? 1 : 0;
			break;
		default:
			break;
		}
		if (score > bestScore) {
			best = i;
			bestScore = score;
		}
	}
	if (best < 0)
		return;
	const Item &potion = player.SpdList[best];
	UseItem(player, potion._iMiscId, potion._iSpell, INVITEM_BELT_FIRST + best);
	player.RemoveSpdBarItem(best);
}

bool IsPathBlockedFor(const Player &player, Point position, Direction dir)
{
	if (IsNoneOf(dir, Direction::North, Direction::East, Direction::South, Direction::West))
		return false;
	const Point leftStep = position + Left(dir);
	const Point rightStep = position + Right(dir);
	if (IsTileNotSolid(leftStep) && IsTileNotSolid(rightStep))
		return false;
	return !PosOkPlayer(player, leftStep) && !PosOkPlayer(player, rightStep);
}

void WalkGuest(Guest &guest, AxisDirection axis)
{
	Player &player = Players[guest.playerId];
	const Direction dir = ToDirection(axis);
	const Point step = player.position.future + dir;

	if (!player.isWalking() && player.CanChangeAction())
		player._pdir = dir;

	const Point anchor = MyPlayer->position.future;
	const int distance = player.position.future.WalkingDistance(anchor);
	const bool leavesLeash = step.WalkingDistance(anchor) > LeashDistance && step.WalkingDistance(anchor) > distance;
	if (leavesLeash || (PosOkPlayer(player, step) && IsPathBlockedFor(player, player.position.future, dir))) {
		if (player._pmode == PM_STAND)
			StartStand(player, dir);
		return;
	}
	SendLoc(guest.playerId, CMD_WALKXY, step);
	guest.walking = true;
}

std::optional<uint32_t> FindGuestSave()
{
	for (uint32_t saveNum = 0; saveNum < MAX_CHARACTERS; saveNum++) {
		if (saveNum == gSaveNumber)
			continue;
		if (std::any_of(Guests.begin(), Guests.end(), [saveNum](const Guest &g) { return g.saveNumber == saveNum; }))
			continue;
		if (OpenSaveArchive(saveNum))
			return saveNum;
	}
	return std::nullopt;
}

// A guest plays another of the user's saved heroes, read only: nothing in
// local co-op writes a guest back, so no save can be overwritten by it.
void LoadGuestHero(Player &player, std::optional<uint32_t> saveNumber, size_t seat)
{
	if (saveNumber) {
		const bool validSaveFile = gbValidSaveFile;
		pfile_read_player_from_save(*saveNumber, player);
		gbValidSaveFile = validSaveFile;
		return;
	}
	// Creating starting items loads and frees the cursor sprites itself, which
	// would otherwise drop the ones pad 1's cursor is using right now.
	const bool cursorLoaded = IsCursorLoaded();
	if (cursorLoaded)
		FreeCursor();
	CreatePlayer(player, gbIsSpawn ? HeroClass::Warrior : HeroClass::Rogue);
	if (cursorLoaded)
		InitCursor();
	CopyUtf8(player._pName, StrCat("Player ", seat + 2), PlayerNameLength);
}

void ClearGuestFromLevel(Player &player)
{
	if (!player.isOnActiveLevel())
		return;
	FixPlrWalkTags(player);
	RemoveEnemyReferences(player);
	RemovePlrMissiles(player);
	if (player.lightId != NO_LIGHT) {
		AddUnLight(player.lightId);
		player.lightId = NO_LIGHT;
	}
	VisionActive[player.getId()] = false;
}

void ReviveGuest(Player &player)
{
	if (!player.hasNoLife() && player._pmode != PM_DEATH)
		return;
	player._pInvincible = false;
	player._pHitPoints = player._pMaxHP;
	player._pHPBase = player._pMaxHPBase;
	player._pmode = PM_STAND;
	CalcPlrInv(player, false);
}

// ---- The guest's personal inventory panel --------------------------------
//
// Each guest has its own panel, its own slot cursor and its own buttons; none
// of it touches pad 1's globals (invflag, pcurs, pcursinvitem, HoldItem), so
// any number of panels can be open at once while the world keeps running.
// Items are moved directly on the guest's Player, as a remote client would do
// to its own hero; nothing here sends a network message as MyPlayer.

constexpr int BeltSlotCount = SLOTXY_BELT_LAST - SLOTXY_BELT_FIRST + 1;
constexpr int GuestBeltTop = SidePanelSize.height + 4;
constexpr int GuestInfoTop = GuestBeltTop + INV_SLOT_SIZE_PX + 6;
constexpr int GuestInfoLineHeight = 16;
constexpr int GuestPanelHeight = GuestInfoTop + 2 * GuestInfoLineHeight + 4;
constexpr int NavRepeatDelay = 6;
constexpr int NavRepeatInterval = 3;

// Panel-relative rectangle of a slot; the belt is a row under the panel.
Rectangle GuestSlotRect(int slot)
{
	if (slot >= SLOTXY_BELT_FIRST) {
		const int i = slot - SLOTXY_BELT_FIRST;
		return { { 17 + i * (INV_SLOT_SIZE_PX + 1), GuestBeltTop }, { INV_SLOT_SIZE_PX + 1, INV_SLOT_SIZE_PX + 1 } };
	}
	return InvRect[slot];
}

// The guest's item under a slot as an inv_item index, or -1 when empty.
int GuestItemAt(const Player &player, int slot)
{
	if (slot <= SLOTXY_EQUIPPED_LAST) {
		if (!player.InvBody[slot].isEmpty())
			return slot;
		// A two-handed weapon also fills the other hand.
		if (slot == SLOTXY_HAND_RIGHT && player.GetItemLocation(player.InvBody[INVLOC_HAND_LEFT]) == ILOC_TWOHAND)
			return INVITEM_HAND_LEFT;
		return -1;
	}
	if (slot <= SLOTXY_INV_LAST) {
		const int8_t cell = player.InvGrid[slot - SLOTXY_INV_FIRST];
		return cell == 0 ? -1 : INVITEM_INV_FIRST + std::abs(cell) - 1;
	}
	const int i = slot - SLOTXY_BELT_FIRST;
	return player.SpdList[i].isEmpty() ? -1 : INVITEM_BELT_FIRST + i;
}

const Item *GuestItem(const Player &player, int invItem)
{
	if (invItem < 0)
		return nullptr;
	if (invItem < INVITEM_INV_FIRST)
		return &player.InvBody[invItem];
	if (invItem <= INVITEM_INV_LAST)
		return &player.InvList[invItem - INVITEM_INV_FIRST];
	return &player.SpdList[invItem - INVITEM_BELT_FIRST];
}

// Steps the slot cursor to the nearest slot in a direction; a multi-cell item
// counts as one stop, so a stick push always lands on something new.
void MoveGuestSlot(Guest &guest, AxisDirection dir)
{
	const Player &player = Players[guest.playerId];
	const int dx = dir.x == AxisDirectionX_LEFT ? -1 : (dir.x == AxisDirectionX_RIGHT ? 1 : 0);
	const int dy = dir.y == AxisDirectionY_UP ? -1 : (dir.y == AxisDirectionY_DOWN ? 1 : 0);
	if (dx == 0 && dy == 0)
		return;
	const Point from = GuestSlotRect(guest.slot).Center();
	const int fromItem = GuestItemAt(player, guest.slot);
	int best = -1;
	int bestScore = 0;
	for (int slot = 0; slot < NUM_XY_SLOTS; slot++) {
		if (slot == guest.slot)
			continue;
		const int item = GuestItemAt(player, slot);
		if (item != -1 && item == fromItem)
			continue;
		const Displacement delta = GuestSlotRect(slot).Center() - from;
		const int along = delta.deltaX * dx + delta.deltaY * dy;
		if (along <= 0)
			continue;
		const int across = std::abs(delta.deltaX * dy - delta.deltaY * dx);
		if (dx != 0 && dy != 0 ? across > along : across > 2 * along)
			continue;
		const int score = along + 2 * across;
		if (best == -1 || score < bestScore) {
			best = slot;
			bestScore = score;
		}
	}
	if (best != -1)
		guest.slot = best;
}

struct InventorySnapshot {
	std::array<Item, NUM_INVLOC> body;
	std::array<Item, InventoryGridCells> list;
	std::array<int8_t, InventoryGridCells> grid;
	int numInv;

	explicit InventorySnapshot(const Player &player)
	    : numInv(player._pNumInv)
	{
		std::copy(std::begin(player.InvBody), std::end(player.InvBody), body.begin());
		std::copy(std::begin(player.InvList), std::end(player.InvList), list.begin());
		std::copy(std::begin(player.InvGrid), std::end(player.InvGrid), grid.begin());
	}

	void Restore(Player &player) const
	{
		std::copy(body.begin(), body.end(), std::begin(player.InvBody));
		std::copy(list.begin(), list.end(), std::begin(player.InvList));
		std::copy(grid.begin(), grid.end(), std::begin(player.InvGrid));
		player._pNumInv = numInv;
	}
};

// Which worn items must come off before this item can go on.
std::vector<inv_body_loc> SlotsToFree(const Player &player, const Item &item)
{
	switch (player.GetItemLocation(item)) {
	case ILOC_HELM:
		return { INVLOC_HEAD };
	case ILOC_ARMOR:
		return { INVLOC_CHEST };
	case ILOC_AMULET:
		return { INVLOC_AMULET };
	case ILOC_RING:
		return { INVLOC_RING_LEFT };
	case ILOC_TWOHAND:
		return { INVLOC_HAND_LEFT, INVLOC_HAND_RIGHT };
	case ILOC_ONEHAND: {
		const Item &left = player.InvBody[INVLOC_HAND_LEFT];
		if (player.GetItemLocation(left) == ILOC_TWOHAND)
			return { INVLOC_HAND_LEFT };
		// Replace the hand that holds the same kind of thing: weapon for weapon, shield for shield.
		for (const inv_body_loc hand : { INVLOC_HAND_LEFT, INVLOC_HAND_RIGHT }) {
			const Item &held = player.InvBody[hand];
			if (!held.isEmpty() && held.isShield() == item.isShield())
				return { hand };
		}
		return { item.isShield() ? INVLOC_HAND_RIGHT : INVLOC_HAND_LEFT };
	}
	default:
		return {};
	}
}

void GuestInventoryChanged(Player &player)
{
	for (Item &item : InventoryPlayerItemsRange { player })
		item.updateRequiredStatsCacheForPlayer(player);
	CalcPlrInv(player, true);
	player.CalcScrolls();
}

// Wears an inventory item, swapping out whatever it replaces; all or nothing.
void GuestEquip(Player &player, int invIndex)
{
	const InventorySnapshot before(player);
	Item item = player.InvList[invIndex];
	item.updateRequiredStatsCacheForPlayer(player);
	if (!item._iStatFlag) {
		player.Say(HeroSpeech::ICantUseThisYet);
		return;
	}
	if (player._pmode > PM_WALK_SIDEWAYS)
		return; // mid-attack or mid-spell; try again in a moment
	player.RemoveInvItem(invIndex, false);
	if (!AutoEquip(player, item, true, false)) {
		for (const inv_body_loc location : SlotsToFree(player, item)) {
			if (player.InvBody[location].isEmpty())
				continue;
			const Item worn = player.InvBody[location];
			RemoveEquipment(player, location, false);
			if (!AutoPlaceItemInInventory(player, worn)) {
				before.Restore(player);
				player.Say(HeroSpeech::IHaveNoRoom);
				return;
			}
		}
		if (!AutoEquip(player, item, true, false)) {
			before.Restore(player);
			player.Say(HeroSpeech::ICantDoThat);
			return;
		}
	}
	GuestInventoryChanged(player);
}

void GuestUnequip(Player &player, int bodyLocation)
{
	const auto location = static_cast<inv_body_loc>(bodyLocation);
	const Item worn = player.InvBody[location];
	RemoveEquipment(player, location, false);
	if (!AutoPlaceItemInInventory(player, worn)) {
		player.InvBody[location] = worn;
		player.Say(HeroSpeech::IHaveNoRoom);
		return;
	}
	GuestInventoryChanged(player);
}

void RemoveGuestItem(Player &player, int invItem)
{
	if (invItem >= INVITEM_BELT_FIRST)
		player.RemoveSpdBarItem(invItem - INVITEM_BELT_FIRST);
	else if (invItem >= INVITEM_INV_FIRST)
		player.RemoveInvItem(invItem - INVITEM_INV_FIRST);
	else
		RemoveEquipment(player, static_cast<inv_body_loc>(invItem), false);
}

// Uses a carried item the way pad 1's right-click would, for the items that
// need no cursor: potions, elixirs, books and the scrolls a guest can aim.
void GuestUseItem(Guest &guest, int invItem)
{
	Player &player = Players[guest.playerId];
	const Item &item = *GuestItem(player, invItem);
	if (!item.isUsable())
		return;
	if (!player.CanUseItem(item)) {
		player.Say(HeroSpeech::ICantUseThisYet);
		return;
	}
	const item_misc_id misc = item._iMiscId;
	const SpellID spell = item._iSpell;
	switch (misc) {
	case IMISC_HEAL:
	case IMISC_FULLHEAL:
	case IMISC_MANA:
	case IMISC_FULLMANA:
	case IMISC_REJUV:
	case IMISC_FULLREJUV:
	case IMISC_ELIXSTR:
	case IMISC_ELIXMAG:
	case IMISC_ELIXDEX:
	case IMISC_ELIXVIT:
	case IMISC_SPECELIX:
		UseItem(player, misc, spell, invItem);
		RemoveGuestItem(player, invItem);
		GuestInventoryChanged(player);
		return;
	case IMISC_BOOK: {
		// UseItem would announce the new spell level as MyPlayer.
		const int8_t id = static_cast<int8_t>(spell);
		if (player._pSplLvl[id] < MaxSpellLevel)
			player._pSplLvl[id]++;
		player._pMemSpells |= GetSpellBitmask(spell);
		if (HasNoneOf(player._pIFlags, ItemSpecialEffect::NoMana)) {
			const Fixed26_6 cost = Fixed26_6::fromInt(GetSpellData(spell).sManaCost);
			player._pMana = std::min(player._pMana + cost, player._pMaxMana);
			player._pManaBase = std::min(player._pManaBase + cost, player._pMaxManaBase);
		}
		RemoveGuestItem(player, invItem);
		GuestInventoryChanged(player);
		if (guest.spell == SpellID::Invalid)
			guest.spell = NextGuestSpell(player, SpellID::Invalid);
		return;
	}
	case IMISC_SCROLL:
	case IMISC_SCROLLT:
		if (!IsAimableByGuest(spell)) {
			player.Say(HeroSpeech::ICantDoThat);
			return;
		}
		// The scroll is used up when the spell actually goes off.
		GuestCast(guest, spell, SpellType::Scroll, invItem);
		return;
	default:
		player.Say(HeroSpeech::ICantDoThat);
		return;
	}
}

// A: wear, take off or use the item under the guest's slot cursor.
void GuestInventoryPrimary(Guest &guest)
{
	Player &player = Players[guest.playerId];
	const int invItem = GuestItemAt(player, guest.slot);
	if (invItem < 0)
		return;
	if (invItem < INVITEM_INV_FIRST) {
		GuestUnequip(player, invItem);
		return;
	}
	const Item &item = *GuestItem(player, invItem);
	if (invItem <= INVITEM_INV_LAST && item.isEquipment()) {
		GuestEquip(player, invItem - INVITEM_INV_FIRST);
		return;
	}
	GuestUseItem(guest, invItem);
}

// Y: move between the inventory and the belt.
void GuestInventoryBelt(Guest &guest)
{
	Player &player = Players[guest.playerId];
	const int invItem = GuestItemAt(player, guest.slot);
	if (invItem < INVITEM_INV_FIRST)
		return;
	const Item item = *GuestItem(player, invItem);
	if (invItem >= INVITEM_BELT_FIRST) {
		if (!AutoPlaceItemInInventory(player, item)) {
			player.Say(HeroSpeech::IHaveNoRoom);
			return;
		}
		player.RemoveSpdBarItem(invItem - INVITEM_BELT_FIRST);
	} else {
		if (!AutoPlaceItemInBelt(player, item, true, false)) {
			player.Say(CanBePlacedOnBelt(player, item) ? HeroSpeech::IHaveNoRoom : HeroSpeech::ICantDoThat);
			return;
		}
		player.RemoveInvItem(invItem - INVITEM_INV_FIRST);
	}
	GuestInventoryChanged(player);
}

// X: drop the item on the ground in front of the guest, through the same
// command a remote player's drop arrives as.
void GuestInventoryDrop(Guest &guest)
{
	Player &player = Players[guest.playerId];
	const int invItem = GuestItemAt(player, guest.slot);
	if (invItem < 0)
		return;
	const Item &item = *GuestItem(player, invItem);
	if (item.IDidx == IDI_EAR)
		return; // ears come from PvP, which an offline couch game has none of
	const std::optional<Point> tile = FindAdjacentPositionForItem(player.position.tile, player._pdir);
	if (!tile) {
		player.Say(HeroSpeech::WhereWouldIPutThis);
		return;
	}
	TCmdPItem cmd {};
	cmd.bCmd = CMD_PUTITEM;
	cmd.x = static_cast<uint8_t>(tile->x);
	cmd.y = static_cast<uint8_t>(tile->y);
	cmd.def.wIndx = static_cast<_item_indexes>(Swap16LE(item.IDidx));
	cmd.def.wCI = Swap16LE(item._iCreateInfo);
	cmd.def.dwSeed = Swap32LE(item._iSeed);
	PrepareItemForNetwork(item, cmd.item);
	Send(guest.playerId, cmd);
	RemoveGuestItem(player, invItem);
	GuestInventoryChanged(player);
}

void StopGuest(Guest &guest)
{
	Player &player = Players[guest.playerId];
	guest.attackHeld = false;
	if (guest.walking) {
		guest.walking = false;
		if (player.walkpath[0] != WALK_NONE && player.destAction == ACTION_NONE)
			SendLoc(guest.playerId, CMD_WALKXY, player.position.future);
	}
}

void SetGuestInventoryOpen(Guest &guest, bool open)
{
	if (guest.inventoryOpen == open)
		return;
	guest.inventoryOpen = open;
	guest.navDirection = { AxisDirectionX_NONE, AxisDirectionY_NONE };
	guest.navTicks = 0;
	if (open)
		StopGuest(guest);
}

// Stick or d-pad: one step at once, then key-repeat while held.
void UpdateGuestInventoryCursor(Guest &guest)
{
	const AxisDirection dir = GuestDirection(guest);
	if (dir.x == AxisDirectionX_NONE && dir.y == AxisDirectionY_NONE) {
		guest.navDirection = dir;
		guest.navTicks = 0;
		return;
	}
	if (dir.x != guest.navDirection.x || dir.y != guest.navDirection.y) {
		guest.navDirection = dir;
		guest.navTicks = 0;
		MoveGuestSlot(guest, dir);
		return;
	}
	guest.navTicks++;
	if (guest.navTicks >= NavRepeatDelay && (guest.navTicks - NavRepeatDelay) % NavRepeatInterval == 0)
		MoveGuestSlot(guest, dir);
}

// Guests' panels sit side by side from the left edge, by seat, so a panel
// never moves when another guest opens or closes theirs. Pad 1's own panel
// stays on the right.
Point GuestPanelOrigin(const Surface &out, size_t seat)
{
	const Point left = GetLeftPanel().position;
	const int x = std::clamp(left.x + static_cast<int>(seat) * SidePanelSize.width, 0, std::max(0, out.w() - SidePanelSize.width));
	const int y = std::clamp(left.y, 0, std::max(0, out.h() - GuestPanelHeight));
	return { x, y };
}

void DrawSlotFrame(const Surface &out, Rectangle rect, uint8_t color)
{
	for (int i = 0; i < 2; i++) {
		DrawHorizontalLine(out, rect.position + Displacement { 0, i }, rect.size.width, color);
		DrawHorizontalLine(out, rect.position + Displacement { 0, rect.size.height - 1 - i }, rect.size.width, color);
		DrawVerticalLine(out, rect.position + Displacement { i, 0 }, rect.size.height, color);
		DrawVerticalLine(out, rect.position + Displacement { rect.size.width - 1 - i, 0 }, rect.size.height, color);
	}
}

UiFlags ItemNameColor(const Item &item)
{
	switch (item._iMagical) {
	case ITEM_QUALITY_MAGIC:
		return UiFlags::ColorBlue;
	case ITEM_QUALITY_UNIQUE:
		return UiFlags::ColorWhitegold;
	default:
		return UiFlags::ColorWhite;
	}
}

void DrawGuestInventory(const Surface &out, const Guest &guest, size_t seat)
{
	const Player &player = Players[guest.playerId];
	const Point origin = GuestPanelOrigin(out, seat);
	const int highlight = GuestItemAt(player, guest.slot);
	DrawInvPanel(out, player, origin, highlight < INVITEM_BELT_FIRST ? highlight : -1);

	DrawHalfTransparentRectTo(out, origin.x, origin.y + SidePanelSize.height, SidePanelSize.width, GuestPanelHeight - SidePanelSize.height);
	for (int i = 0; i < BeltSlotCount; i++) {
		const Rectangle rect = GuestSlotRect(SLOTXY_BELT_FIRST + i);
		const Point position = origin + Displacement { rect.position.x, rect.position.y + INV_SLOT_SIZE_PX };
		const Item &item = player.SpdList[i];
		if (item.isEmpty())
			continue;
		InvDrawSlotBack(out, position, InventorySlotSizeInPixels, item._iMagical);
		const ClxSprite sprite = GetInvItemSprite(item._iCurs + CURSOR_FIRSTITEM);
		if (highlight == INVITEM_BELT_FIRST + i)
			ClxDrawOutline(out, GetOutlineColor(item, true), position, sprite);
		DrawItem(item, out, position, sprite);
	}

	const Rectangle slotRect = GuestSlotRect(guest.slot);
	DrawSlotFrame(out, { origin + Displacement { slotRect.position.x, slotRect.position.y }, slotRect.size }, PAL16_YELLOW + 2);

	const Item *item = GuestItem(player, highlight);
	const std::string title = item != nullptr && !item->isEmpty()
	    ? FormatRuntime("P{:d} {:s}: {:s}", seat + 2, player._pName, std::string_view(item->getName()))
	    : FormatRuntime("P{:d} {:s}", seat + 2, player._pName);
	const Point info = origin + Displacement { 0, GuestInfoTop };
	DrawString(out, title, { info, { SidePanelSize.width, GuestInfoLineHeight } },
	    { .flags = UiFlags::AlignCenter | UiFlags::FontSize12 | (item != nullptr && !item->isEmpty() ? ItemNameColor(*item) : UiFlags::ColorWhite) });
	DrawString(out, _("A use/wear  X drop  Y belt  B close"), { info + Displacement { 0, GuestInfoLineHeight }, { SidePanelSize.width, GuestInfoLineHeight } },
	    { .flags = UiFlags::AlignCenter | UiFlags::FontSize12 | UiFlags::ColorWhite });
}

} // namespace

bool IsLocalCoopAvailable()
{
	return gbIsMultiplayer && IsLoopback && MyPlayer != nullptr;
}

size_t LocalGuestCount()
{
	return Guests.size();
}

bool IsLocalGuest(const Player &player)
{
	for (const Guest &guest : Guests) {
		if (&Players[guest.playerId] == &player)
			return true;
	}
	return false;
}

bool IsLocalPlayer(const Player &player)
{
	return &player == MyPlayer || (!Guests.empty() && IsLocalGuest(player));
}

int LocalGuestOnDevice(SDL_JoystickID device)
{
	const Guest *guest = FindGuest(device);
	return guest != nullptr ? guest->playerId : -1;
}

void SendGuestCommand(uint8_t playerId, const std::byte *data, size_t size)
{
	PendingCommands.push_back({ playerId, std::vector<std::byte>(data, data + size) });
}

void ProcessGuestCommands()
{
	std::vector<PendingCommand> commands;
	commands.swap(PendingCommands);
	for (const PendingCommand &command : commands) {
		if (!IsLocalGuest(Players[command.playerId]))
			continue; // the guest left after queuing it
		for (size_t offset = 0; offset < command.data.size();) {
			const size_t parsed = ParseCmd(command.playerId, reinterpret_cast<const TCmd *>(&command.data[offset]), command.data.size() - offset);
			if (parsed == 0)
				break;
			offset += parsed;
		}
	}
}

bool JoinLocalGuest(SDL_JoystickID device)
{
	if (!IsLocalCoopAvailable() || device == -1 || FindGuest(device) != nullptr || Guests.size() >= MaxLocalGuests)
		return false;

	std::optional<uint8_t> freeId;
	for (size_t i = 0; i < Players.size(); i++) {
		if (i != MyPlayerId && !Players[i].plractive) {
			freeId = static_cast<uint8_t>(i);
			break;
		}
	}
	if (!freeId)
		return false;

	Player &player = Players[*freeId];
	const Player &myPlayer = *MyPlayer;
	const std::optional<uint32_t> saveNumber = FindGuestSave();
	player = { };
	LoadGuestHero(player, saveNumber, Guests.size());

	player.plrlevel = myPlayer.plrlevel;
	player.plrIsOnSetLevel = myPlayer.plrIsOnSetLevel;
	player.position.tile = myPlayer.position.tile;
	player.position.future = myPlayer.position.tile;
	player.position.old = myPlayer.position.tile;
	player._pLvlChanging = false;
	player.plractive = true;
	gbActivePlayers++;

	Guests.push_back({ *freeId, device, saveNumber });
	SetControllerDeviceClaimed(device, true);
	Guest &guest = Guests.back();

	if (player.isOnActiveLevel()) {
		InitPlayerGFX(player);
		InitPlayer(player, true);
		SyncInitPlrPos(player);
		ChangeLightXY(player.lightId, player.position.tile);
		ChangeVisionXY(player.getId(), player.position.tile);
		StartStand(player, Direction::South);
	}
	guest.spell = NextGuestSpell(player, SpellID::Invalid);

	EventPlrMsg(FormatRuntime(_("{:s} joined on controller {:d}"), player._pName, Guests.size() + 1));
	return true;
}

void RemoveLocalGuest(SDL_JoystickID device)
{
	const auto it = std::find_if(Guests.begin(), Guests.end(), [device](const Guest &g) { return g.device == device; });
	if (it == Guests.end())
		return;
	Player &player = Players[it->playerId];
	SetControllerDeviceClaimed(device, false);
	Guests.erase(it);

	ClearGuestFromLevel(player);
	EventPlrMsg(FormatRuntime(_("{:s} left the game"), player._pName));
	player.plractive = false;
	player._pName[0] = '\0';
	ResetPlayerGFX(player);
	if (gbActivePlayers > 0)
		gbActivePlayers--;
}

void ResetLocalGuests()
{
	for (const Guest &guest : Guests)
		SetControllerDeviceClaimed(guest.device, false);
	Guests.clear();
	PendingCommands.clear();
	PrimaryDevice = -1;
}

bool HandleLocalCoopEvent(const SDL_Event &event)
{
#ifndef USE_SDL1
	if (!IsLocalCoopAvailable())
		return false;
	const SDL_JoystickID device = ControllerEventDevice(event);
	if (device == -1)
		return false;

	Guest *guest = FindGuest(device);
	if (guest == nullptr) {
		for (const ControllerButtonEvent ctrlEvent : ToControllerButtonEvents(event)) {
			if (ctrlEvent.button == ControllerButton_NONE || ctrlEvent.button == ControllerButton_IGNORE || ctrlEvent.up)
				continue;
			// The first pad pressed in a game is pad 1. Any other pad joins
			// with Start; until then it still drives pad 1, as upstream.
			if (PrimaryDevice == -1 || !IsDeviceConnected(PrimaryDevice)) {
				PrimaryDevice = device;
				return false;
			}
			if (device != PrimaryDevice && ctrlEvent.button == ControllerButton_BUTTON_START)
				return JoinLocalGuest(device);
		}
		return false;
	}

	Player &player = Players[guest->playerId];
	for (const ControllerButtonEvent ctrlEvent : ToControllerButtonEvents(event)) {
		if (ctrlEvent.button == ControllerButton_BUTTON_A)
			guest->attackHeld = !ctrlEvent.up && !guest->inventoryOpen;
		if (ctrlEvent.up || !player.plractive || !player.isOnActiveLevel() || player.hasNoLife() || player._pmode == PM_DEATH)
			continue;
		if (ctrlEvent.button == ControllerButton_BUTTON_BACK) {
			SetGuestInventoryOpen(*guest, !guest->inventoryOpen);
			continue;
		}
		if (guest->inventoryOpen) {
			switch (ctrlEvent.button) {
			case ControllerButton_BUTTON_A:
				GuestInventoryPrimary(*guest);
				break;
			case ControllerButton_BUTTON_X:
				GuestInventoryDrop(*guest);
				break;
			case ControllerButton_BUTTON_Y:
				GuestInventoryBelt(*guest);
				break;
			case ControllerButton_BUTTON_B:
				SetGuestInventoryOpen(*guest, false);
				break;
			default:
				break;
			}
			continue;
		}
		switch (ctrlEvent.button) {
		case ControllerButton_BUTTON_A:
			GuestPrimaryAction(*guest);
			break;
		case ControllerButton_BUTTON_X:
			GuestCastSpell(*guest);
			break;
		case ControllerButton_BUTTON_Y:
			guest->spell = NextGuestSpell(player, guest->spell);
			break;
		case ControllerButton_BUTTON_B:
			GuestDrinkPotion(*guest);
			break;
		default:
			break;
		}
	}
	return true;
#else
	(void)event;
	return false;
#endif
}

void UpdateLocalGuests()
{
	for (size_t i = 0; i < Guests.size();) {
		if (!IsDeviceConnected(Guests[i].device)) {
			RemoveLocalGuest(Guests[i].device);
			continue;
		}
		i++;
	}

	for (Guest &guest : Guests) {
		Player &player = Players[guest.playerId];
		if (!player.plractive || !player.isOnActiveLevel() || player.hasNoLife() || player._pmode == PM_DEATH) {
			SetGuestInventoryOpen(guest, false);
			continue;
		}
		if (guest.inventoryOpen) {
			UpdateGuestInventoryCursor(guest);
			continue;
		}
		if (player._pInvincible)
			continue;
		const AxisDirection dir = GuestDirection(guest);
		if (dir.x != AxisDirectionX_NONE || dir.y != AxisDirectionY_NONE) {
			WalkGuest(guest, dir);
			continue;
		}
		if (guest.walking) {
			guest.walking = false;
			if (player.walkpath[0] != WALK_NONE && player.destAction == ACTION_NONE)
				SendLoc(guest.playerId, CMD_WALKXY, player.position.future); // stop
		}
		if (guest.attackHeld && player._pmode == PM_STAND && player.destAction == ACTION_NONE && player.walkpath[0] == WALK_NONE)
			GuestPrimaryAction(guest);
	}
}

void MoveLocalGuestsToMyLevel()
{
	const Player &myPlayer = *MyPlayer;
	for (const Guest &guest : Guests) {
		Player &player = Players[guest.playerId];
		ClrPlrPath(player);
		player.destAction = ACTION_NONE;
		player.plrlevel = myPlayer.plrlevel;
		player.plrIsOnSetLevel = myPlayer.plrIsOnSetLevel;
		player._pLvlChanging = false;
		player._pLvlVisited[player.plrlevel] = true;
		player.lightId = NO_LIGHT;
		// A fallen guest gets back up when the party changes level.
		ReviveGuest(player);
	}
}

void PlaceLocalGuestsAfterLevelLoad(bool keepPositions)
{
	const Player &myPlayer = *MyPlayer;
	for (const Guest &guest : Guests) {
		Player &player = Players[guest.playerId];
		if (!player.plractive || !player.isOnActiveLevel())
			continue;
		if (!keepPositions) {
			player.position.tile = myPlayer.position.tile;
			player.position.future = myPlayer.position.tile;
			SyncInitPlrPos(player);
		}
		SetPlayerOld(player);
		ChangeLightXY(player.lightId, player.position.tile);
		ChangeVisionXY(player.getId(), player.position.tile);
		StartStand(player, player._pdir);
	}
}

bool IsGuestInventoryOpen(const Player &player)
{
	for (const Guest &guest : Guests) {
		if (&Players[guest.playerId] == &player)
			return guest.inventoryOpen;
	}
	return false;
}

int GuestInventorySlot(const Player &player)
{
	for (const Guest &guest : Guests) {
		if (&Players[guest.playerId] == &player)
			return guest.slot;
	}
	return -1;
}

void DrawLocalGuestInventories(const Surface &out)
{
	for (size_t seat = 0; seat < Guests.size(); seat++) {
		const Guest &guest = Guests[seat];
		const Player &player = Players[guest.playerId];
		if (guest.inventoryOpen && player.plractive && player.isOnActiveLevel())
			DrawGuestInventory(out, guest, seat);
	}
}

void DrawLocalCoopHud(const Surface &out)
{
	if (Guests.empty())
		return;
	constexpr int LineHeight = 18;
	constexpr int Width = 560;
	const int x = (out.w() - Width) / 2;
	int y = 4;
	for (size_t i = 0; i < Guests.size(); i++) {
		const Player &player = Players[Guests[i].playerId];
		const SpellID spell = Guests[i].spell;
		const std::string spellName = spell == SpellID::Invalid ? std::string(_("no spell")) : std::string(pgettext("spell", GetSpellData(spell).sNameText));
		std::string text;
		if (player.hasNoLife() || player._pmode == PM_DEATH) {
			text = FormatRuntime(_("P{:d} {:s}: fallen, back on the next level"), i + 2, player._pName);
		} else {
			text = FormatRuntime(_("P{:d} {:s}  Life {:d}/{:d}  Mana {:d}/{:d}  X: {:s}"),
			    i + 2, player._pName, player._pHitPoints.whole(), player._pMaxHP.whole(),
			    player._pMana.whole(), player._pMaxMana.whole(), spellName);
		}
		DrawHalfTransparentRectTo(out, x, y, Width, LineHeight);
		DrawString(out, text, { { x, y + 2 }, { Width, LineHeight } },
		    { .flags = UiFlags::AlignCenter | UiFlags::FontSize12 | (player.hasNoLife() ? UiFlags::ColorRed : UiFlags::ColorWhite) });
		y += LineHeight + 2;
	}
}

} // namespace devilution
