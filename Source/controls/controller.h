#pragma once

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#else
#include <SDL.h>
#endif

#include "controller_buttons.h"
#include "utils/static_vector.hpp"

namespace devilution {

// Must be called exactly once at the start of each SDL input event.
void UnlockControllerState(const SDL_Event &event);

StaticVector<ControllerButtonEvent, 4> ToControllerButtonEvents(const SDL_Event &event);

bool IsControllerButtonPressed(ControllerButton button);
bool IsControllerButtonComboPressed(ControllerButtonCombo combo);

// Local co-op needs to ask about one assigned device instead of "any device".
// A device is an SDL joystick instance; both its gamepad and its raw joystick
// registration are consulted, matching the any-device query for that device.
// The keyboard controller is not a device here: it stays with the solo path.
bool IsControllerButtonPressedOnDevice(SDL_JoystickID device, ControllerButton button);
// Both buttons must be held on the same device, so a modifier held on one pad
// can never combine with a button on another.
bool IsControllerButtonComboPressedOnDevice(SDL_JoystickID device, ControllerButtonCombo combo);

// The SDL joystick instance that produced a controller event, or -1 for the
// keyboard controller and for events from no registered device.
SDL_JoystickID ControllerEventDevice(const SDL_Event &event);

// A claimed device belongs to a local co-op guest. The solo path (any-device
// queries, stick motion) then ignores it, so a guest's held button or stick can
// never act for the main player.
void SetControllerDeviceClaimed(SDL_JoystickID device, bool claimed);
bool IsControllerDeviceClaimed(SDL_JoystickID device);
bool IsControllerEventFromClaimedDevice(const SDL_Event &event);

bool HandleControllerAddedOrRemovedEvent(const SDL_Event &event);

} // namespace devilution
