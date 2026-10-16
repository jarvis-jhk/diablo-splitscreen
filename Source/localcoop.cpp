#include "localcoop.hpp"

#include <algorithm>
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
#include "cursor.h"
#include "engine/point.hpp"
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
bool IsGuestSpell(const Player &player, SpellID spell)
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
	if (!IsValidSpell(spell))
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

void GuestCastSpell(Guest &guest)
{
	Player &player = Players[guest.playerId];
	const SpellID spell = guest.spell;
	if (!IsGuestSpell(player, spell)) {
		player.Say(HeroSpeech::IDontHaveASpellReady);
		return;
	}
	if (leveltype == DTYPE_TOWN && !GetSpellData(spell).isAllowedInTown()) {
		player.Say(HeroSpeech::ICantCastThatHere);
		return;
	}
	const SpellCheckResult check = CheckSpell(player, spell, SpellType::Spell, false);
	if (check == SpellCheckResult::Fail_NoMana) {
		player.Say(HeroSpeech::NotEnoughMana);
		return;
	}
	if (check != SpellCheckResult::Success)
		return;

	const auto spellParam = static_cast<uint16_t>(static_cast<int8_t>(spell));
	const auto typeParam = static_cast<uint16_t>(static_cast<uint8_t>(SpellType::Spell));
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
		cmd.wParam4 = 0;
		Send(guest.playerId, cmd);
	} else if (monster) {
		TCmdParam4 cmd;
		cmd.bCmd = CMD_SPELLID;
		cmd.wParam1 = Swap16LE(static_cast<uint16_t>(*monster));
		cmd.wParam2 = Swap16LE(spellParam);
		cmd.wParam3 = Swap16LE(typeParam);
		cmd.wParam4 = 0;
		Send(guest.playerId, cmd);
	} else {
		const Point target = AimPoint(player, 4);
		TCmdLocParam3 cmd;
		cmd.bCmd = CMD_SPELLXY;
		cmd.x = static_cast<uint8_t>(target.x);
		cmd.y = static_cast<uint8_t>(target.y);
		cmd.wParam1 = Swap16LE(spellParam);
		cmd.wParam2 = Swap16LE(typeParam);
		cmd.wParam3 = 0;
		Send(guest.playerId, cmd);
	}
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
			guest->attackHeld = !ctrlEvent.up;
		if (ctrlEvent.up || !player.plractive || !player.isOnActiveLevel() || player.hasNoLife() || player._pmode == PM_DEATH)
			continue;
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
		if (!player.plractive || !player.isOnActiveLevel() || player.hasNoLife() || player._pmode == PM_DEATH || player._pInvincible)
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
