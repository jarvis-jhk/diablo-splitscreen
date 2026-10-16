#include <gtest/gtest.h>

#include <SDL.h>

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "capture.h"
#include "controls/controller.h"
#include "controls/devices/game_controller.h"
#include "controls/devices/joystick.h"
#include "cursor.h"
#include "diablo.h"
#include "engine/assets.hpp"
#include "engine/backbuffer_state.hpp"
#include "engine/events.hpp"
#include "engine/palette.h"
#include "engine/render/scrollrt.h"
#include "engine/sound.h"
#include "game_mode.hpp"
#include "gmenu.h"
#include "init.hpp"
#include "inv.h"
#include "items.h"
#include "levels/dun_tile_data.hpp"
#include "levels/gendung.h"
#include "levels/tile_properties.hpp"
#include "localcoop.hpp"
#include "lua/lua_global.hpp"
#include "missiles.h"
#include "monster.h"
#include "multi.h"
#include "options.h"
#include "pfile.h"
#include "player.h"
#include "qol/monhealthbar.h"
#include "qol/xpbar.h"
#include "spells.h"
#include "storm/storm_net.hpp"
#include "tables/monstdat.h"
#include "tables/playerdat.hpp"
#include "tables/spelldat.h"
#include "utils/display.h"
#include "utils/paths.h"
#include "utils/ui_fwd.h"
#include "utils/utf8.hpp"

namespace devilution {
void TestGameEventHandler(const SDL_Event &event, uint16_t modState);
void TestGameLogic();
} // namespace devilution

using namespace devilution;

namespace {

// Two SDL2 virtual gamepads drive a real offline multiplayer game through the
// production event path (FetchMessage -> GameEventHandler -> GameLogic) on
// spawn.mpq. Virtual pads say nothing about physical controllers or the Deck.
class VirtualPad {
public:
	VirtualPad()
	{
		const int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
		EXPECT_GE(index, 0) << SDL_GetError();
		id_ = SDL_JoystickGetDeviceInstanceID(index);
		handle_ = SDL_JoystickOpen(index);
	}

	~VirtualPad()
	{
		Unplug();
	}

	void Unplug()
	{
		if (handle_ == nullptr)
			return;
		SDL_JoystickClose(handle_);
		handle_ = nullptr;
		for (int index = 0; index < SDL_NumJoysticks(); ++index) {
			if (SDL_JoystickGetDeviceInstanceID(index) == id_) {
				SDL_JoystickDetachVirtual(index);
				break;
			}
		}
	}

	void Button(SDL_GameControllerButton button, bool down)
	{
		SDL_JoystickSetVirtualButton(handle_, button, down ? 1 : 0);
	}

	void Axis(SDL_GameControllerAxis axis, int16_t value)
	{
		SDL_JoystickSetVirtualAxis(handle_, axis, value);
	}

	SDL_JoystickID id() const { return id_; }

private:
	SDL_JoystickID id_ = -1;
	SDL_Joystick *handle_ = nullptr;
};

void Pump()
{
	SDL_Event event;
	uint16_t modState;
	while (FetchMessage(&event, &modState)) {
		if (event.type != 0)
			TestGameEventHandler(event, modState);
	}
}

void Tick(int count = 1)
{
	for (int i = 0; i < count; i++) {
		Pump();
		ProcessGameMessagePackets();
		TestGameLogic();
	}
}

void Press(VirtualPad &pad, SDL_GameControllerButton button)
{
	pad.Button(button, true);
	Tick();
	pad.Button(button, false);
	Tick();
}

template <typename Predicate>
bool TickUntil(Predicate predicate, int maxTicks)
{
	for (int i = 0; i < maxTicks; i++) {
		if (predicate())
			return true;
		Tick();
	}
	return predicate();
}

// Pad 1 walks through the ordinary upstream path; some directions may be
// walled off at the stairs, so try each until pad 1's hero has moved.
bool WalkPadOne(VirtualPad &pad)
{
	const Point start = MyPlayer->position.tile;
	for (const auto [axis, value] : { std::pair { SDL_CONTROLLER_AXIS_LEFTX, 32767 }, std::pair { SDL_CONTROLLER_AXIS_LEFTY, 32767 },
	         std::pair { SDL_CONTROLLER_AXIS_LEFTX, -32767 }, std::pair { SDL_CONTROLLER_AXIS_LEFTY, -32767 } }) {
		pad.Axis(axis, static_cast<int16_t>(value));
		const bool moved = TickUntil([&] { return MyPlayer->position.tile != start; }, 40);
		pad.Axis(axis, 0);
		Tick(20);
		if (moved)
			return true;
	}
	return false;
}

// With LOCALCOOP_RENDER_DIR set, the same scenarios run through the real
// renderer (not HeadlessMode) and save screenshots there.
std::string RenderDir;

void Snapshot(const std::string &name)
{
	if (RenderDir.empty())
		return;
	const std::filesystem::path pref = paths::PrefPath();
	for (const auto &entry : std::filesystem::directory_iterator(pref)) {
		if (entry.path().filename().string().rfind("Screenshot", 0) == 0)
			std::filesystem::remove(entry.path());
	}
	gbRunGame = true; // DrawAndBlit only draws a running game
	RedrawEverything();
	CaptureScreen();
	gbRunGame = false;
	for (const auto &entry : std::filesystem::directory_iterator(pref)) {
		if (entry.path().filename().string().rfind("Screenshot", 0) == 0) {
			std::filesystem::copy_file(entry.path(), std::filesystem::path(RenderDir) / (name + ".png"), std::filesystem::copy_options::overwrite_existing);
			std::filesystem::remove(entry.path());
		}
	}
}

Player &Guest()
{
	for (Player &player : Players) {
		if (IsLocalGuest(player))
			return player;
	}
	ADD_FAILURE() << "no guest";
	return Players[1];
}

void LoadLevel(uint8_t level)
{
	FreeGameMem();
	setlevel = false;
	currlevel = level;
	leveltype = GetLevelType(level);
	MyPlayer->setLevel(level);
	ASSERT_TRUE(LoadGameLevel(level == 0, ENTRY_MAIN).has_value());
	if (!RenderDir.empty())
		UpdateSystemPalette(logical_palette);
}

// Moves an existing level monster onto a free tile next to the target.
Monster &MonsterNextTo(Point target, Point awayFrom)
{
	Monster *chosen = nullptr;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &monster = Monsters[ActiveMonsters[i]];
		if (monster.getId() >= MAX_PLRS && !monster.isPlayerMinion() && !monster.hasNoLife() && monster.uniqueType == UniqueMonsterType::None && monster.exp(sgGameInitInfo.nDifficulty) > 0) {
			chosen = &monster;
			break;
		}
	}
	EXPECT_NE(chosen, nullptr);
	Monster &monster = *chosen;
	std::optional<Point> free;
	int bestDistance = 0;
	for (const Direction dir : { Direction::South, Direction::SouthWest, Direction::West, Direction::NorthWest, Direction::North, Direction::NorthEast, Direction::East, Direction::SouthEast }) {
		const Point p = target + dir;
		if (IsTileSolid(p) || dMonster[p.x][p.y] != 0 || dPlayer[p.x][p.y] != 0 || dObject[p.x][p.y] != 0)
			continue;
		const int distance = p.WalkingDistance(awayFrom);
		if (!free || distance > bestDistance) {
			free = p;
			bestDistance = distance;
		}
	}
	EXPECT_TRUE(free.has_value());
	dMonster[monster.position.tile.x][monster.position.tile.y] = 0;
	dMonster[monster.position.future.x][monster.position.future.y] = 0;
	monster.position.tile = *free;
	monster.position.future = *free;
	monster.position.old = *free;
	dMonster[free->x][free->y] = static_cast<int16_t>(monster.getId() + 1);
	M_StartStand(monster, GetDirection(*free, target));
	return monster;
}

class LocalCoopTest : public ::testing::Test {
public:
	static void SetUpTestSuite()
	{
		if (const char *dir = SDL_getenv("LOCALCOOP_RENDER_DIR"); dir != nullptr && *dir != '\0') {
			RenderDir = dir;
			HeadlessMode = false;
		}
		ASSERT_EQ(SDL_Init(SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER | (RenderDir.empty() ? 0 : SDL_INIT_VIDEO)), 0) << SDL_GetError();
		LoadCoreArchives();
		LoadGameArchives();
		HaveData = HaveMainData();
		if (!HaveData)
			return;

		SaveDir = (std::filesystem::temp_directory_path() / "devilutionx-localcoop-test/").string();
		std::filesystem::remove_all(SaveDir);
		std::filesystem::create_directories(SaveDir);
		paths::SetPrefPath(SaveDir);
		paths::SetConfigPath(SaveDir);

		InitKeymapActions();
		InitPadmapActions();
		LoadOptions();
		LuaInitialize();
		gbIsSpawn = true;
		gbIsHellfire = false;
		gbMusicOn = false;
		gbSoundOn = false;
		LoadSpellData();
		LoadPlayerDataFiles();
		LoadMissileData();
		LoadMonsterData();
		LoadItemData();
		LoadObjectData();
		if (!RenderDir.empty()) {
			forceResolution = { 1280, 800 };
			init_create_window();
			InitItemGFX();
		} else {
			AdjustToScreenGeometry({ 640, 480 }); // the camera needs a screen size
		}

		// One saved hero besides pad 1's (save 0), for the join panel to offer;
		// a multiplayer hero, the kind an offline multiplayer game lists.
		gbIsMultiplayer = true;
		Players.resize(1);
		_uiheroinfo saved { };
		saved.saveNumber = 1;
		CopyUtf8(saved.name, "Saved", sizeof(saved.name));
		saved.heroclass = HeroClass::Warrior;
		ASSERT_TRUE(pfile_ui_save_create(&saved));
		Players.clear();
		gbIsMultiplayer = false;
	}

	static void TearDownTestSuite()
	{
		if (HaveData) {
			LuaShutdown();
			std::filesystem::remove_all(SaveDir);
		}
		SDL_Quit();
	}

protected:
	void SetUp() override
	{
		if (!HaveData)
			GTEST_SKIP() << "MPQ assets (spawn.mpq or DIABDAT.MPQ) not found - skipping test";
		StartOfflineMultiplayerGame();
	}

	void TearDown() override
	{
		if (!HaveData)
			return;
		padB_.reset();
		padA_.reset();
		Tick();
		FreeGameMem();
		FreeCursor();
		ResetLocalGuests();
		gbIsMultiplayer = false;
		SNetLeaveGame(leaveinfo_t::LEAVE_EXIT);
		Players.clear();
		MyPlayer = nullptr;
	}

	// What NetInit and SetupLocalPositions do for "Multi Player > Offline",
	// without the menus: a loopback provider, four player slots, pad 1's hero.
	void StartOfflineMultiplayerGame()
	{
		gbIsMultiplayer = true;
		InitGameInfo();
		GameData gameData { };
		gameData.size = sizeof(gameData);
		// Outside HeadlessMode this would open the hero selection menu.
		const bool headless = HeadlessMode;
		HeadlessMode = true;
		const bool provider = SNetInitializeProvider(SELCONN_LOOPBACK, &gameData);
		HeadlessMode = headless;
		ASSERT_TRUE(provider);
		int playerId = 0;
		ASSERT_TRUE(SNetCreateGame("local", "local", reinterpret_cast<char *>(&gameData), sizeof(gameData), &playerId));
		ASSERT_TRUE(IsLoopback);

		Players.clear();
		Players.resize(MAX_PLRS);
		MyPlayerId = 0;
		MyPlayer = &Players[0];
		InspectPlayer = MyPlayer;
		CreatePlayer(*MyPlayer, HeroClass::Warrior);
		CopyUtf8(MyPlayer->_pName, "PadOne", PlayerNameLength);
		MyPlayer->plractive = true;
		MyPlayer->position.tile = { 75, 68 };
		MyPlayer->position.future = MyPlayer->position.tile;
		for (uint32_t i = 0; i < NUMLEVELS; i++)
			DungeonSeeds[i] = 0x5eed0000 + i;

		CalcViewportGeometry();
		InitCursor(); // after CreatePlayer, which loads and frees it itself
		gmenu_init_menu();
		if (!RenderDir.empty()) {
			InitMonsterHealthBar();
			InitXPBar();
		}
		LoadLevel(0);
		InitLevelCursor();
		gbProcessPlayers = true; // RunGameLoop: Diablo is alive
		Tick();

		padA_.emplace();
		padB_.emplace();
		Tick();
		ASSERT_NE(GameController::Get(padA_->id()), nullptr);
		ASSERT_NE(GameController::Get(padB_->id()), nullptr);
	}

	// Start opens the join panel; the guest is a new Warrior, so every case
	// starts from the same fresh hero.
	void JoinGuest()
	{
		Press(*padA_, SDL_CONTROLLER_BUTTON_BACK); // pad 1 is the first pad pressed; Back alone does nothing
		Press(*padB_, SDL_CONTROLLER_BUTTON_START);
		ASSERT_TRUE(IsLocalJoinPending(padB_->id()));
		Press(*padB_, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
		const std::vector<std::string> choices = LocalJoinChoices(padB_->id());
		ASSERT_EQ(LocalJoinSelection(padB_->id()), 1);
		ASSERT_EQ(choices[1], "New Warrior");
		Press(*padB_, SDL_CONTROLLER_BUTTON_A);
		ASSERT_EQ(LocalGuestCount(), 1u);
	}

	void EnterDungeon()
	{
		LoadLevel(1);
		Tick(3);
	}

	static inline bool HaveData = false;
	static inline std::string SaveDir;
	std::optional<VirtualPad> padA_;
	std::optional<VirtualPad> padB_;
};

TEST_F(LocalCoopTest, StartOnASecondPadJoinsAGuestBesidePadOne)
{
	JoinGuest();
	const Player &guest = Guest();
	EXPECT_NE(&guest, MyPlayer);
	EXPECT_TRUE(guest.plractive);
	EXPECT_TRUE(guest.isOnActiveLevel());
	EXPECT_LE(guest.position.tile.WalkingDistance(MyPlayer->position.tile), 2);
	EXPECT_EQ(LocalGuestOnDevice(padB_->id()), guest.getId());
	EXPECT_TRUE(IsControllerDeviceClaimed(padB_->id()));
	EXPECT_FALSE(IsControllerDeviceClaimed(padA_->id()));
	// The guest's Start never reached pad 1's game menu.
	EXPECT_FALSE(gmenu_is_active());
	Snapshot("00-guest-joined-in-town");
}

TEST_F(LocalCoopTest, StartOnPadOneStillOpensItsMenuAndJoinsNobody)
{
	Press(*padA_, SDL_CONTROLLER_BUTTON_START);
	EXPECT_EQ(LocalGuestCount(), 0u);
	// Back+Start is pad 1's game menu, unchanged.
	padA_->Button(SDL_CONTROLLER_BUTTON_BACK, true);
	Tick();
	Press(*padA_, SDL_CONTROLLER_BUTTON_START);
	padA_->Button(SDL_CONTROLLER_BUTTON_BACK, false);
	Tick();
	EXPECT_EQ(LocalGuestCount(), 0u);
	EXPECT_TRUE(gmenu_is_active());
}

TEST_F(LocalCoopTest, EachPadMovesOnlyItsOwnHero)
{
	JoinGuest();
	EnterDungeon();
	Player &guest = Guest();
	const Point guestStart = guest.position.tile;
	const Point myStart = MyPlayer->position.tile;

	padB_->Axis(SDL_CONTROLLER_AXIS_LEFTX, 32767);
	ASSERT_TRUE(TickUntil([&] { return guest.position.tile != guestStart; }, 60));
	Tick(20);
	EXPECT_EQ(MyPlayer->position.tile, myStart);
	EXPECT_EQ(MyPlayer->walkpath[0], WALK_NONE);
	padB_->Axis(SDL_CONTROLLER_AXIS_LEFTX, 0);
	Tick(20);
	const Point guestStop = guest.position.tile;

	ASSERT_TRUE(WalkPadOne(*padA_));
	EXPECT_EQ(guest.position.tile, guestStop);
}

// Where a hero's feet are drawn this frame, after the camera has recentred.
Point ScreenOf(const Player &player)
{
	UpdateSharedCamera();
	return GetScreenPosition(player.position.tile);
}

bool IsInView(Point screen)
{
	return screen.x >= 0 && screen.x < gnScreenWidth && screen.y >= 0 && screen.y < gnViewportHeight;
}

void HoldStick(VirtualPad &pad, int16_t x, int16_t y, int ticks)
{
	pad.Axis(SDL_CONTROLLER_AXIS_LEFTX, x);
	pad.Axis(SDL_CONTROLLER_AXIS_LEFTY, y);
	Tick(ticks);
	pad.Axis(SDL_CONTROLLER_AXIS_LEFTX, 0);
	pad.Axis(SDL_CONTROLLER_AXIS_LEFTY, 0);
	Tick(20);
}

TEST_F(LocalCoopTest, TheCameraCentresOnThePartyNotOnPadOne)
{
	const Point centre = ScreenOf(*MyPlayer); // upstream: pad 1's hero at the centre
	JoinGuest();
	ASSERT_TRUE(IsSharedCameraActive());
	Player &guest = Guest();
	const Point guestStart = guest.position.tile;
	HoldStick(*padB_, 32767, 0, 60);
	ASSERT_GE(guest.position.tile.WalkingDistance(guestStart), 2);

	const Point mine = ScreenOf(*MyPlayer);
	const Point theirs = ScreenOf(guest);
	const Point middle { (mine.x + theirs.x) / 2, (mine.y + theirs.y) / 2 };
	EXPECT_LE(std::abs(middle.x - centre.x), TILE_WIDTH / 2) << "party centre on screen x";
	EXPECT_LE(std::abs(middle.y - centre.y), TILE_HEIGHT / 2) << "party centre on screen y";
	EXPECT_GT(std::abs(mine.x - centre.x) + std::abs(mine.y - centre.y), TILE_WIDTH / 2) << "pad 1 is off centre";
	Snapshot("05-shared-camera-centres-on-both");
}

TEST_F(LocalCoopTest, NoHeroCanWalkOffTheSharedScreen)
{
	JoinGuest();
	Player &guest = Guest();
	// From the town start the open ground runs up-right and down-left: the
	// guest runs up-right as far as it can, then pad 1 runs down-left until the
	// screen (at 640x480, not a wall) holds it back.
	HoldStick(*padB_, 32767, -32767, 400);
	EXPECT_TRUE(IsInView(ScreenOf(guest)));
	EXPECT_TRUE(IsInView(ScreenOf(*MyPlayer)));
	const Point myStart = MyPlayer->position.tile;
	HoldStick(*padA_, -32767, 32767, 400);
	EXPECT_NE(MyPlayer->position.tile, myStart);
	const Point mine = ScreenOf(*MyPlayer);
	const Point theirs = ScreenOf(guest);
	EXPECT_TRUE(IsInView(mine)) << mine.x << "," << mine.y;
	EXPECT_TRUE(IsInView(theirs)) << theirs.x << "," << theirs.y;
	EXPECT_GE(theirs.x - mine.x, 8 * TILE_WIDTH / 2) << "further apart than the old 7-tile leash";
	if (RenderDir.empty()) {
		EXPECT_FALSE(SharedCameraAllowsStep(guest, guest.position.future + Direction::NorthEast));
		EXPECT_FALSE(SharedCameraAllowsStep(*MyPlayer, MyPlayer->position.future + Direction::SouthWest));
		EXPECT_TRUE(IsTileNotSolid(MyPlayer->position.future + Direction::SouthWest)) << "the screen stopped pad 1, not a wall";
	}
	Snapshot("06-shared-camera-party-spread-out");

	// Closing up is always allowed.
	const Point guestStop = guest.position.tile;
	HoldStick(*padB_, -32767, 32767, 40);
	EXPECT_NE(guest.position.tile, guestStop);
}

TEST_F(LocalCoopTest, WithoutGuestsTheCameraIsUpstreams)
{
	EXPECT_FALSE(IsSharedCameraActive());
	ASSERT_TRUE(WalkPadOne(*padA_));
	EXPECT_EQ(ViewPosition, MyPlayer->position.tile);
}

TEST_F(LocalCoopTest, HeldGuestButtonsNeverReachPadOne)
{
	JoinGuest();
	padB_->Button(SDL_CONTROLLER_BUTTON_X, true);
	padB_->Axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 32767);
	Tick(2);
	EXPECT_FALSE(IsControllerButtonPressed(ControllerButton_BUTTON_X));
	EXPECT_FALSE(invflag); // pad 1's inventory trigger, held on the guest pad
	padA_->Button(SDL_CONTROLLER_BUTTON_X, true);
	Tick(2);
	EXPECT_TRUE(IsControllerButtonPressed(ControllerButton_BUTTON_X));
	padA_->Button(SDL_CONTROLLER_BUTTON_X, false);
	padB_->Button(SDL_CONTROLLER_BUTTON_X, false);
	padB_->Axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 0);
	Tick(2);
}

TEST_F(LocalCoopTest, GuestFightsAndGainsExperienceWhilePadOneInventoryIsOpen)
{
	JoinGuest();
	EnterDungeon();
	Player &guest = Guest();

	padA_->Axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 32767);
	Tick(2);
	padA_->Axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 0);
	Tick(2);
	ASSERT_TRUE(invflag);

	Monster &monster = MonsterNextTo(guest.position.tile, MyPlayer->position.tile);
	monster.hitPoints = Fixed26_6::fromInt(1);
	Tick(2);
	Snapshot("01-pad1-inventory-open-guest-next-to-monster");
	const uint32_t myExperience = MyPlayer->_pExperience;
	ASSERT_EQ(guest._pExperience, 0u);

	padB_->Button(SDL_CONTROLLER_BUTTON_A, true); // held: keeps attacking
	const bool killed = TickUntil([&] { return monster.hasNoLife() || monster.mode == MonsterMode::Death; }, 600);
	padB_->Button(SDL_CONTROLLER_BUTTON_A, false);
	Tick(2);
	ASSERT_TRUE(killed);
	Snapshot("02-guest-killed-monster-while-pad1-inventory-open");
	EXPECT_GT(guest._pExperience, 0u);
	EXPECT_EQ(MyPlayer->_pExperience, myExperience);
	EXPECT_TRUE(invflag) << "pad 1's inventory stayed open throughout";
}

TEST_F(LocalCoopTest, MonstersHurtTheGuestNotPadOne)
{
	JoinGuest();
	EnterDungeon();
	Player &guest = Guest();
	// Keep pad 1 out of reach so the monster's only target is the guest.
	Monster &monster = MonsterNextTo(guest.position.tile, MyPlayer->position.tile);
	monster.hitPoints = monster.maxHitPoints = Fixed26_6::fromInt(500);
	const Fixed26_6 myLife = MyPlayer->_pHitPoints;
	const Fixed26_6 guestLife = guest._pHitPoints;
	ASSERT_TRUE(TickUntil([&] { return guest._pHitPoints < guestLife; }, 1500));
	Snapshot("04-monster-hurts-guest");
	EXPECT_EQ(MyPlayer->_pHitPoints, myLife);
}

TEST_F(LocalCoopTest, GuestCastsItsOwnSpellWithItsOwnMana)
{
	JoinGuest();
	EnterDungeon();
	Player &guest = Guest();
	guest._pMemSpells |= GetSpellBitmask(SpellID::Firebolt);
	guest._pSplLvl[static_cast<int8_t>(SpellID::Firebolt)] = 1;
	guest._pMana = guest._pMaxMana = guest._pManaBase = guest._pMaxManaBase = Fixed26_6::fromInt(50);
	const Fixed26_6 myMana = MyPlayer->_pMana;

	Press(*padB_, SDL_CONTROLLER_BUTTON_Y); // select the newly learned spell
	Press(*padB_, SDL_CONTROLLER_BUTTON_X);
	const auto guestFirebolt = [&] {
		for (Missile &missile : Missiles) {
			if (missile._mitype == MissileID::Firebolt && missile.sourcePlayer() == &guest)
				return true;
		}
		return false;
	};
	ASSERT_TRUE(TickUntil(guestFirebolt, 60));
	Tick(3);
	Snapshot("03-guest-casts-firebolt");
	EXPECT_LT(guest._pMana, Fixed26_6::fromInt(50));
	EXPECT_EQ(MyPlayer->_pMana, myMana);
}

int Possessions(const Player &player)
{
	int count = player._pNumInv + player._pGold;
	for (const Item &item : player.SpdList)
		count += item.isEmpty() ? 0 : 1;
	for (const Item &item : player.InvBody)
		count += item.isEmpty() ? 0 : 1;
	return count;
}

TEST_F(LocalCoopTest, GuestPicksUpAnItemIntoItsOwnInventory)
{
	JoinGuest(); // in town: no monster to attack first
	Player &guest = Guest();
	const int guestBefore = Possessions(guest);
	const int mineBefore = Possessions(*MyPlayer);
	CreateRndItem(guest.position.tile + Direction::SouthEast, /*onlygood=*/true, false, false); // town gold is worth 0
	Tick();
	padB_->Button(SDL_CONTROLLER_BUTTON_A, true);
	const bool picked = TickUntil([&] { return Possessions(guest) > guestBefore; }, 200);
	padB_->Button(SDL_CONTROLLER_BUTTON_A, false);
	Tick();
	EXPECT_TRUE(picked);
	EXPECT_EQ(Possessions(*MyPlayer), mineBefore);
}

TEST_F(LocalCoopTest, GuestFollowsPadOneToTheNextLevel)
{
	JoinGuest();
	EnterDungeon();
	Player &guest = Guest();
	EXPECT_TRUE(guest.isOnLevel(1));
	EXPECT_LE(guest.position.tile.WalkingDistance(MyPlayer->position.tile), 2);
	EXPECT_EQ(dPlayer[guest.position.tile.x][guest.position.tile.y], guest.getId() + 1);
}

TEST_F(LocalCoopTest, UnpluggingTheGuestPadRemovesOnlyTheGuest)
{
	JoinGuest();
	EnterDungeon();
	const uint8_t guestId = Guest().getId();
	padB_->Button(SDL_CONTROLLER_BUTTON_A, true);
	Tick();
	padB_->Unplug();
	Tick(3);
	EXPECT_EQ(LocalGuestCount(), 0u);
	EXPECT_FALSE(Players[guestId].plractive);
	EXPECT_TRUE(MyPlayer->plractive);

	EXPECT_TRUE(WalkPadOne(*padA_));
}

// ---- Personal inventories ----------------------------------------------

void OpenPadOneInventory(VirtualPad &pad)
{
	pad.Axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 32767);
	Tick(2);
	pad.Axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 0);
	Tick(2);
}

// Panel-relative centre of a guest slot; the guest's belt is a row under its panel.
Point GuestSlotCenter(int slot)
{
	if (slot >= SLOTXY_BELT_FIRST)
		return { 17 + (slot - SLOTXY_BELT_FIRST) * (INV_SLOT_SIZE_PX + 1) + 14, 352 + 4 + 14 };
	return InvRect[slot].Center();
}

int InventoryIndexAt(const Player &player, int slot)
{
	if (slot < SLOTXY_INV_FIRST || slot > SLOTXY_INV_LAST)
		return -1;
	return std::abs(player.InvGrid[slot - SLOTXY_INV_FIRST]) - 1;
}

std::optional<int> FindInventoryItem(const Player &player, _item_indexes id)
{
	for (int i = 0; i < player._pNumInv; i++) {
		if (player.InvList[i].IDidx == id)
			return i;
	}
	return std::nullopt;
}

// Steers the guest's slot cursor with its d-pad, the way a player would.
template <typename IsTarget>
bool SteerGuestCursor(VirtualPad &pad, const Player &guest, Point target, IsTarget isTarget)
{
	for (int step = 0; step < 30; step++) {
		const int slot = GuestInventorySlot(guest);
		if (isTarget(slot))
			return true;
		const Displacement d = target - GuestSlotCenter(slot);
		SDL_GameControllerButton button;
		if (std::abs(d.deltaX) >= std::abs(d.deltaY))
			button = d.deltaX > 0 ? SDL_CONTROLLER_BUTTON_DPAD_RIGHT : SDL_CONTROLLER_BUTTON_DPAD_LEFT;
		else
			button = d.deltaY > 0 ? SDL_CONTROLLER_BUTTON_DPAD_DOWN : SDL_CONTROLLER_BUTTON_DPAD_UP;
		Press(pad, button);
	}
	return isTarget(GuestInventorySlot(guest));
}

bool SteerGuestToSlot(VirtualPad &pad, const Player &guest, int slot)
{
	return SteerGuestCursor(pad, guest, GuestSlotCenter(slot), [slot](int s) { return s == slot; });
}

bool SteerGuestToInventoryItem(VirtualPad &pad, const Player &guest, int index)
{
	for (int cell = 0; cell < InventoryGridCells; cell++) {
		if (std::abs(guest.InvGrid[cell]) - 1 == index) {
			return SteerGuestCursor(pad, guest, GuestSlotCenter(SLOTXY_INV_FIRST + cell),
			    [&guest, index](int s) { return InventoryIndexAt(guest, s) == index; });
		}
	}
	return false;
}

TEST_F(LocalCoopTest, BothInventoriesOpenWithIndependentCursorsWhileTheWorldRuns)
{
	JoinGuest();
	EnterDungeon();
	Player &guest = Guest();
	OpenPadOneInventory(*padA_);
	ASSERT_TRUE(invflag);

	Press(*padB_, SDL_CONTROLLER_BUTTON_BACK);
	ASSERT_TRUE(IsGuestInventoryOpen(guest));
	EXPECT_TRUE(invflag) << "the guest's Back never reached pad 1";
	EXPECT_FALSE(gmenu_is_active());

	// The guest's d-pad moves only the guest's slot cursor...
	const Point mouse = MousePosition;
	const Point guestTile = guest.position.tile;
	const int startSlot = GuestInventorySlot(guest);
	Press(*padB_, SDL_CONTROLLER_BUTTON_DPAD_UP);
	EXPECT_NE(GuestInventorySlot(guest), startSlot);
	EXPECT_EQ(MousePosition, mouse);
	EXPECT_EQ(guest.position.tile, guestTile);

	// ...and pad 1's d-pad and trigger never touch the guest's panel.
	const int guestSlot = GuestInventorySlot(guest);
	Press(*padA_, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
	Tick(5);
	EXPECT_EQ(GuestInventorySlot(guest), guestSlot);
	EXPECT_TRUE(invflag);
	EXPECT_TRUE(IsGuestInventoryOpen(guest));
	Snapshot("05-both-inventories-open");

	// Nothing is paused: a monster still walks up and hits the guest.
	Monster &monster = MonsterNextTo(guest.position.tile, MyPlayer->position.tile);
	monster.hitPoints = monster.maxHitPoints = Fixed26_6::fromInt(500);
	const Fixed26_6 guestLife = guest._pHitPoints;
	ASSERT_TRUE(TickUntil([&] { return guest._pHitPoints < guestLife; }, 1500));
	EXPECT_TRUE(invflag);
	EXPECT_TRUE(IsGuestInventoryOpen(guest));

	Press(*padB_, SDL_CONTROLLER_BUTTON_B);
	EXPECT_FALSE(IsGuestInventoryOpen(guest));
	EXPECT_TRUE(invflag);
}

TEST_F(LocalCoopTest, PadOneWalksWhileTheGuestSortsItsInventory)
{
	JoinGuest();
	EnterDungeon();
	Player &guest = Guest();
	Press(*padB_, SDL_CONTROLLER_BUTTON_BACK);
	ASSERT_TRUE(IsGuestInventoryOpen(guest));

	// The guest's stick steers its cursor (with key-repeat), not its hero.
	const Point guestTile = guest.position.tile;
	padB_->Axis(SDL_CONTROLLER_AXIS_LEFTX, 32767);
	Tick(20);
	padB_->Axis(SDL_CONTROLLER_AXIS_LEFTX, 0);
	Tick(2);
	EXPECT_GE(GuestInventorySlot(guest), SLOTXY_INV_FIRST + 3) << "held stick repeats";
	EXPECT_EQ(guest.position.tile, guestTile);

	EXPECT_TRUE(WalkPadOne(*padA_));
	EXPECT_FALSE(invflag);
	EXPECT_TRUE(IsGuestInventoryOpen(guest));
}

TEST_F(LocalCoopTest, GuestWearsUsesBeltsAndDropsItsOwnItems)
{
	JoinGuest(); // town: nothing attacks while it sorts
	Player &guest = Guest();
	ASSERT_EQ(guest._pClass, HeroClass::Warrior);
	const int mine = Possessions(*MyPlayer);
	const _item_indexes myWeapon = MyPlayer->InvBody[INVLOC_HAND_LEFT].IDidx;
	Press(*padB_, SDL_CONTROLLER_BUTTON_BACK);
	ASSERT_TRUE(IsGuestInventoryOpen(guest));

	// Wear the club from the pack: it swaps with the sword in the weapon hand.
	const std::optional<int> club = FindInventoryItem(guest, IDI_WARRCLUB);
	ASSERT_TRUE(club.has_value());
	ASSERT_EQ(guest.InvBody[INVLOC_HAND_LEFT].IDidx, IDI_WARRIOR);
	ASSERT_TRUE(SteerGuestToInventoryItem(*padB_, guest, *club));
	Press(*padB_, SDL_CONTROLLER_BUTTON_A);
	EXPECT_EQ(guest.InvBody[INVLOC_HAND_LEFT].IDidx, IDI_WARRCLUB);
	EXPECT_EQ(guest.InvBody[INVLOC_HAND_RIGHT].IDidx, IDI_WARRSHLD);
	EXPECT_TRUE(FindInventoryItem(guest, IDI_WARRIOR).has_value());
	EXPECT_FALSE(FindInventoryItem(guest, IDI_WARRCLUB).has_value());
	Snapshot("06-guest-wears-club");

	// Take the club off again from the equipment slot.
	ASSERT_TRUE(SteerGuestToSlot(*padB_, guest, SLOTXY_HAND_LEFT));
	Press(*padB_, SDL_CONTROLLER_BUTTON_A);
	EXPECT_TRUE(guest.InvBody[INVLOC_HAND_LEFT].isEmpty());
	EXPECT_TRUE(FindInventoryItem(guest, IDI_WARRCLUB).has_value());

	// Drink a belt potion.
	ASSERT_FALSE(guest.SpdList[0].isEmpty());
	guest._pHPBase -= guest._pHitPoints - Fixed26_6::fromInt(5);
	guest._pHitPoints = Fixed26_6::fromInt(5);
	const Fixed26_6 myLife = MyPlayer->_pHitPoints;
	ASSERT_TRUE(SteerGuestToSlot(*padB_, guest, SLOTXY_BELT_FIRST));
	Press(*padB_, SDL_CONTROLLER_BUTTON_A);
	EXPECT_GT(guest._pHitPoints, Fixed26_6::fromInt(5));
	EXPECT_TRUE(guest.SpdList[0].isEmpty());
	EXPECT_EQ(MyPlayer->_pHitPoints, myLife);

	// Y moves the other potion off the belt into the pack, and back.
	ASSERT_FALSE(guest.SpdList[1].isEmpty());
	ASSERT_TRUE(SteerGuestToSlot(*padB_, guest, SLOTXY_BELT_FIRST + 1));
	Press(*padB_, SDL_CONTROLLER_BUTTON_Y);
	EXPECT_TRUE(guest.SpdList[1].isEmpty());
	const std::optional<int> potion = FindInventoryItem(guest, IDI_HEAL);
	ASSERT_TRUE(potion.has_value());
	ASSERT_TRUE(SteerGuestToInventoryItem(*padB_, guest, *potion));
	Press(*padB_, SDL_CONTROLLER_BUTTON_Y);
	EXPECT_FALSE(FindInventoryItem(guest, IDI_HEAL).has_value());
	EXPECT_FALSE(guest.SpdList[0].isEmpty());

	// X drops the sword on the ground; the guest can pick it up again.
	const std::optional<int> sword = FindInventoryItem(guest, IDI_WARRIOR);
	ASSERT_TRUE(sword.has_value());
	const int carried = Possessions(guest);
	ASSERT_TRUE(SteerGuestToInventoryItem(*padB_, guest, *sword));
	Press(*padB_, SDL_CONTROLLER_BUTTON_X);
	Tick(2);
	EXPECT_EQ(Possessions(guest), carried - 1);
	EXPECT_FALSE(FindInventoryItem(guest, IDI_WARRIOR).has_value());
	Press(*padB_, SDL_CONTROLLER_BUTTON_B);
	padB_->Button(SDL_CONTROLLER_BUTTON_A, true);
	EXPECT_TRUE(TickUntil([&] { return Possessions(guest) == carried; }, 200)); // auto-equipped into the free hand
	padB_->Button(SDL_CONTROLLER_BUTTON_A, false);
	Tick();

	// Pad 1's hero was never touched.
	EXPECT_EQ(Possessions(*MyPlayer), mine);
	EXPECT_EQ(MyPlayer->InvBody[INVLOC_HAND_LEFT].IDidx, myWeapon);
	EXPECT_FALSE(invflag);
}

} // namespace

// ---- Controller lobby ------------------------------------------------------

TEST_F(LocalCoopTest, StartOpensAJoinPanelOfferingSavedHeroesAndNewClasses)
{
	Press(*padA_, SDL_CONTROLLER_BUTTON_BACK);
	Press(*padB_, SDL_CONTROLLER_BUTTON_START);
	EXPECT_TRUE(IsLocalJoinPending(padB_->id()));
	EXPECT_EQ(LocalGuestCount(), 0u);
	EXPECT_TRUE(IsControllerDeviceClaimed(padB_->id()));
	EXPECT_FALSE(gmenu_is_active());
	// spawn.mpq allows the Warrior only; pad 1's own save 0 is not offered.
	const std::vector<std::string> expected { "Saved, level 1 Warrior", "New Warrior" };
	EXPECT_EQ(LocalJoinChoices(padB_->id()), expected);
	EXPECT_EQ(LocalJoinSelection(padB_->id()), 0);
	Snapshot("20-join-panel");

	// The stick wraps around the list and moves nothing in the world.
	const Point myStart = MyPlayer->position.tile;
	Press(*padB_, SDL_CONTROLLER_BUTTON_DPAD_UP);
	EXPECT_EQ(LocalJoinSelection(padB_->id()), 1);
	Press(*padB_, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
	EXPECT_EQ(LocalJoinSelection(padB_->id()), 0);
	EXPECT_EQ(MyPlayer->position.tile, myStart);

	Press(*padB_, SDL_CONTROLLER_BUTTON_A);
	EXPECT_FALSE(IsLocalJoinPending(padB_->id()));
	ASSERT_EQ(LocalGuestCount(), 1u);
	EXPECT_STREQ(Guest()._pName, "Saved");
	EXPECT_STREQ(MyPlayer->_pName, "PadOne");
	EXPECT_EQ(LocalGuestOnDevice(padB_->id()), Guest().getId());
	EXPECT_EQ(MyPlayer->walkpath[0], WALK_NONE); // the confirming A never reached pad 1
}

TEST_F(LocalCoopTest, BCancelsTheJoinPanelAndReleasesThePad)
{
	Press(*padA_, SDL_CONTROLLER_BUTTON_BACK);
	Press(*padB_, SDL_CONTROLLER_BUTTON_START);
	ASSERT_TRUE(IsLocalJoinPending(padB_->id()));
	Press(*padB_, SDL_CONTROLLER_BUTTON_B);
	EXPECT_FALSE(IsLocalJoinPending(padB_->id()));
	EXPECT_FALSE(IsControllerDeviceClaimed(padB_->id()));
	EXPECT_EQ(LocalGuestCount(), 0u);

	Press(*padB_, SDL_CONTROLLER_BUTTON_START);
	EXPECT_TRUE(IsLocalJoinPending(padB_->id()));
	padB_->Unplug();
	Tick(3);
	EXPECT_FALSE(IsLocalJoinPending(padB_->id()));
	EXPECT_EQ(LocalGuestCount(), 0u);
}

TEST_F(LocalCoopTest, ASavedHeroInPlayIsNotOfferedToTheNextPad)
{
	Press(*padA_, SDL_CONTROLLER_BUTTON_BACK);
	Press(*padB_, SDL_CONTROLLER_BUTTON_START);
	Press(*padB_, SDL_CONTROLLER_BUTTON_A); // "Saved"
	ASSERT_EQ(LocalGuestCount(), 1u);

	VirtualPad padC;
	Tick();
	Press(padC, SDL_CONTROLLER_BUTTON_START);
	ASSERT_TRUE(IsLocalJoinPending(padC.id()));
	const std::vector<std::string> expected { "New Warrior" };
	EXPECT_EQ(LocalJoinChoices(padC.id()), expected);
	Press(padC, SDL_CONTROLLER_BUTTON_A);
	ASSERT_EQ(LocalGuestCount(), 2u);
	EXPECT_STREQ(Players[LocalGuestOnDevice(padC.id())]._pName, "Player 3");
	padC.Unplug();
	Tick(3);
	EXPECT_EQ(LocalGuestCount(), 1u);
}

TEST_F(LocalCoopTest, StartThenALetsAGuestLeaveAndFreesItsHero)
{
	Press(*padA_, SDL_CONTROLLER_BUTTON_BACK);
	Press(*padB_, SDL_CONTROLLER_BUTTON_START);
	Press(*padB_, SDL_CONTROLLER_BUTTON_A); // "Saved"
	ASSERT_EQ(LocalGuestCount(), 1u);
	EnterDungeon();
	Player &guest = Guest();
	const uint8_t guestId = guest.getId();

	// Start asks first; while it asks, the guest's stick moves nobody.
	Press(*padB_, SDL_CONTROLLER_BUTTON_START);
	EXPECT_TRUE(IsLocalGuestLeaveMenuOpen(padB_->id()));
	EXPECT_FALSE(gmenu_is_active());
	const Point guestStart = guest.position.tile;
	padB_->Axis(SDL_CONTROLLER_AXIS_LEFTX, 32767);
	Tick(30);
	padB_->Axis(SDL_CONTROLLER_AXIS_LEFTX, 0);
	Tick();
	EXPECT_EQ(guest.position.tile, guestStart);
	Snapshot("21-leave-question");
	Press(*padB_, SDL_CONTROLLER_BUTTON_B); // stay
	EXPECT_FALSE(IsLocalGuestLeaveMenuOpen(padB_->id()));
	EXPECT_EQ(LocalGuestCount(), 1u);

	Press(*padB_, SDL_CONTROLLER_BUTTON_START);
	Press(*padB_, SDL_CONTROLLER_BUTTON_A); // leave
	EXPECT_EQ(LocalGuestCount(), 0u);
	EXPECT_FALSE(Players[guestId].plractive);
	EXPECT_FALSE(IsControllerDeviceClaimed(padB_->id()));
	EXPECT_TRUE(MyPlayer->plractive);
	EXPECT_EQ(MyPlayer->walkpath[0], WALK_NONE);

	// The freed hero is offered again.
	Press(*padB_, SDL_CONTROLLER_BUTTON_START);
	ASSERT_TRUE(IsLocalJoinPending(padB_->id()));
	EXPECT_EQ(LocalJoinChoices(padB_->id()).front(), "Saved, level 1 Warrior");
}
