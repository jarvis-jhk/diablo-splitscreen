#include "controls/controller.h"

#include <cmath>

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#else
#include <SDL.h>
#endif

#ifndef USE_SDL1
#include "controls/devices/game_controller.h"
#endif
#include "controls/devices/joystick.h"
#include "controls/devices/kbcontroller.h"
#include "engine/demomode.h"
#include "utils/sdl_compat.h"

namespace devilution {

void UnlockControllerState(const SDL_Event &event)
{
#ifndef USE_SDL1
	GameController *const controller = GameController::Get(event);
	if (controller != nullptr) {
		controller->UnlockTriggerState();
	}
#endif
	Joystick *const joystick = Joystick::Get(event);
	if (joystick != nullptr) {
		joystick->UnlockHatState();
	}
}

StaticVector<ControllerButtonEvent, 4> ToControllerButtonEvents(const SDL_Event &event)
{
	ControllerButtonEvent result { ControllerButton_NONE, false };
	switch (event.type) {
	case SDL_EVENT_JOYSTICK_BUTTON_UP:
	case SDL_EVENT_KEY_UP:
#ifndef USE_SDL1
	case SDL_EVENT_GAMEPAD_BUTTON_UP:
#endif
		result.up = true;
		break;
	default:
		break;
	}
#if HAS_KBCTRL == 1
	if (!demo::IsRunning()) {
		result.button = KbCtrlToControllerButton(event);
		if (result.button != ControllerButton_NONE)
			return { result };
	}
#endif
#ifndef USE_SDL1
	GameController *const controller = GameController::Get(event);
	if (controller != nullptr) {
		result.button = controller->ToControllerButton(event);
		if (result.button != ControllerButton_NONE) {
			if (result.button == ControllerButton_AXIS_TRIGGERLEFT || result.button == ControllerButton_AXIS_TRIGGERRIGHT) {
				result.up = !controller->IsPressed(result.button);
			}
			return { result };
		}
	}
#endif

	const Joystick *joystick = Joystick::Get(event);
	if (joystick != nullptr) {
		return devilution::Joystick::ToControllerButtonEvents(event);
	}

	return { result };
}

bool IsControllerButtonPressed(ControllerButton button)
{
#ifndef USE_SDL1
	if (GameController::IsPressedOnAnyController(button))
		return true;
#endif
#if HAS_KBCTRL == 1
	if (!demo::IsRunning() && IsKbCtrlButtonPressed(button))
		return true;
#endif
	return Joystick::IsPressedOnAnyJoystick(button);
}

bool IsControllerButtonComboPressed(ControllerButtonCombo combo)
{
	return IsControllerButtonPressed(combo.button)
	    && (combo.modifier == ControllerButton_NONE || IsControllerButtonPressed(combo.modifier));
}

bool IsControllerButtonPressedOnDevice(SDL_JoystickID device, ControllerButton button)
{
#ifndef USE_SDL1
	const GameController *const controller = GameController::Get(device);
	if (controller != nullptr && controller->IsPressed(button))
		return true;
#endif
	const Joystick *const joystick = Joystick::Get(device);
	return joystick != nullptr && joystick->IsPressed(button);
}

bool IsControllerButtonComboPressedOnDevice(SDL_JoystickID device, ControllerButtonCombo combo)
{
	return IsControllerButtonPressedOnDevice(device, combo.button)
	    && (combo.modifier == ControllerButton_NONE || IsControllerButtonPressedOnDevice(device, combo.modifier));
}

SDL_JoystickID ControllerEventDevice(const SDL_Event &event)
{
#if HAS_KBCTRL == 1
	if (!demo::IsRunning() && KbCtrlToControllerButton(event) != ControllerButton_NONE)
		return -1;
#endif
#ifndef USE_SDL1
	const GameController *const controller = GameController::Get(event);
	if (controller != nullptr)
		return controller->instance_id();
#endif
	const Joystick *const joystick = Joystick::Get(event);
	if (joystick != nullptr)
		return joystick->instance_id();
	return -1;
}

bool HandleControllerAddedOrRemovedEvent(const SDL_Event &event)
{
#ifndef USE_SDL1
	switch (event.type) {
	case SDL_EVENT_GAMEPAD_ADDED:
		GameController::Add(SDLC_EventGamepadDevice(event).which);
		break;
	case SDL_EVENT_GAMEPAD_REMOVED:
		GameController::Remove(SDLC_EventGamepadDevice(event).which);
		break;
	case SDL_EVENT_JOYSTICK_ADDED:
		Joystick::Add(event.jdevice.which);
		break;
	case SDL_EVENT_JOYSTICK_REMOVED:
		Joystick::Remove(event.jdevice.which);
		break;
	default:
		return false;
	}
	return true;
#else
	return false;
#endif
}

} // namespace devilution
