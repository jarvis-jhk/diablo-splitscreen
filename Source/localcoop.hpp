#pragma once

#include <cstddef>
#include <cstdint>

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
/** @brief MyPlayer, or a guest played on this machine. */
bool IsLocalPlayer(const Player &player);
/** @brief Player id of the guest on a device, or -1. */
int LocalGuestOnDevice(SDL_JoystickID device);

/** @brief Queue a command that will be parsed with the guest as sender. */
void SendGuestCommand(uint8_t playerId, const std::byte *data, size_t size);
void ProcessGuestCommands();

bool JoinLocalGuest(SDL_JoystickID device);
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

} // namespace devilution
