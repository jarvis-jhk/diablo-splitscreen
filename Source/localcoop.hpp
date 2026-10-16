#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "engine/displacement.hpp"
#include "engine/point.hpp"

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#else
#include <SDL.h>
#endif

namespace devilution {

struct Player;
struct Surface;

// Local co-op: extra heroes ("guests") in the same offline multiplayer game,
// each played with one assigned controller on the shared screen. Pad 1, the
// keyboard and the mouse keep the ordinary MyPlayer path untouched; a guest's
// device is claimed so none of its input reaches that path.
//
// A guest's commands are parsed exactly like a remote player's, but with the
// guest as sender. Effects that a network client applies only to "its own"
// player (damage taken, experience, pickups) use IsLocalPlayer, so this process
// is authoritative for every local hero. Guests exist only in offline games:
// there is no remote client that could disagree.

constexpr size_t MaxLocalGuests = 3;

/** @brief An offline multiplayer game, the only kind that accepts guests. */
bool IsLocalCoopAvailable();
size_t LocalGuestCount();
bool IsLocalGuest(const Player &player);
/** @brief The guest's pad was unplugged; its hero stands until Start on a free pad. */
bool IsLocalGuestWaitingForPad(const Player &player);
/** @brief MyPlayer, or a guest played on this machine. */
bool IsLocalPlayer(const Player &player);
/** @brief Player id of the guest on a device, or -1. */
int LocalGuestOnDevice(SDL_JoystickID device);

/** @brief Queue a command that will be parsed with the guest as sender. */
void SendGuestCommand(uint8_t playerId, const std::byte *data, size_t size);
void ProcessGuestCommands();

/**
 * @brief Join at once with the first hero the join panel would offer.
 * Start on a free pad opens the panel instead; this is for tests and tools.
 */
bool JoinLocalGuest(SDL_JoystickID device);
/** @brief A pad that pressed Start and is choosing its hero in the join panel. */
bool IsLocalJoinPending(SDL_JoystickID device);
/** @brief The join panel's entries for that pad, in order; empty if none. */
std::vector<std::string> LocalJoinChoices(SDL_JoystickID device);
/** @brief The entry the pad's join panel has selected, or -1. */
int LocalJoinSelection(SDL_JoystickID device);
/** @brief Start on a guest's pad asked "leave the game?" and it is waiting. */
bool IsLocalGuestLeaveMenuOpen(SDL_JoystickID device);
void RemoveLocalGuest(SDL_JoystickID device);
/** @brief Forget every guest and claimed device (game end). */
void ResetLocalGuests();

/** @brief Returns true if the event belonged to a guest (or joined one). */
bool HandleLocalCoopEvent(const SDL_Event &event);
/** @brief Continuous guest input; call once per game tick. */
void UpdateLocalGuests();
/** @brief Guests follow MyPlayer to the level about to be loaded. */
void MoveLocalGuestsToMyLevel();
/** @brief Place guests beside MyPlayer after a level load. */
void PlaceLocalGuestsAfterLevelLoad(bool keepPositions);
void DrawLocalCoopHud(const Surface &out);

// Shared camera: while guests play, the view centres on the middle of every
// living hero on the level instead of on MyPlayer, and no hero may take a step
// that would push the party wider than the screen.
bool IsSharedCameraActive();
/** @brief Recentre on the party; call once per frame before drawing. Sets ViewPosition. */
void UpdateSharedCamera();
/**
 * @brief Screen offset from ViewPosition to the party centre, in the sign of a
 * walking camera offset; used in place of MyPlayer's walking offset.
 */
Displacement SharedCameraOffset();
/** @brief False if a local hero stepping to that tile would leave the shared screen. */
bool SharedCameraAllowsStep(const Player &player, Point to);

// A guest's own inventory panel: Back opens and closes it on the guest's pad.
bool IsGuestInventoryOpen(const Player &player);
/** @brief The guest's slot cursor (an inv_xy_slot), or -1 for a non-guest. */
int GuestInventorySlot(const Player &player);
void DrawLocalGuestInventories(const Surface &out);

} // namespace devilution
