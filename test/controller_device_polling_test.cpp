#include <gtest/gtest.h>

#include <SDL.h>

#include <vector>

#include "controls/controller.h"
#include "controls/devices/game_controller.h"
#include "controls/devices/joystick.h"

namespace devilution {
namespace {

// Local co-op asks "is this button held on THIS pad". These tests drive two
// real SDL2 virtual gamepads; they say nothing about physical controllers.
class ControllerDevicePollingTest : public testing::Test {
protected:
	void SetUp() override
	{
		ASSERT_TRUE(GameController::All().empty());
		ASSERT_TRUE(Joystick::All().empty());
		ASSERT_EQ(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER), 0) << SDL_GetError();
	}

	void TearDown() override
	{
		for (const SDL_JoystickID id : devices_) {
			GameController::Remove(id);
			Joystick::Remove(id);
			const int index = DeviceIndex(id);
			if (index >= 0)
				EXPECT_EQ(SDL_JoystickDetachVirtual(index), 0) << SDL_GetError();
		}
		for (SDL_Joystick *handle : handles_)
			SDL_JoystickClose(handle);
		SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
	}

	SDL_JoystickID Attach(SDL_JoystickType type = SDL_JOYSTICK_TYPE_GAMECONTROLLER)
	{
		const int index = SDL_JoystickAttachVirtual(type, SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 1);
		EXPECT_GE(index, 0) << SDL_GetError();
		if (index < 0)
			return -1;
		const SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(index);
		devices_.push_back(id);
		// Our own reference keeps the virtual device writable even after the
		// game drops its registration, as an unplugged-but-still-held pad would.
		handles_.push_back(SDL_JoystickOpen(index));
		return id;
	}

	static void Register(SDL_JoystickID id, bool gamepad = true, bool joystick = true)
	{
		const int index = DeviceIndex(id);
		ASSERT_GE(index, 0);
		if (gamepad)
			GameController::Add(index);
		if (joystick)
			Joystick::Add(index);
	}

	void Hold(SDL_JoystickID id, SDL_GameControllerButton button, bool down = true)
	{
		ASSERT_EQ(SDL_JoystickSetVirtualButton(ownHandle_(id), button, down ? 1 : 0), 0) << SDL_GetError();
		SDL_JoystickUpdate();
	}

	static int DeviceIndex(SDL_JoystickID id)
	{
		for (int index = 0; index < SDL_NumJoysticks(); ++index) {
			if (SDL_JoystickGetDeviceInstanceID(index) == id)
				return index;
		}
		return -1;
	}

private:
	SDL_Joystick *ownHandle_(SDL_JoystickID id)
	{
		for (size_t i = 0; i < devices_.size(); ++i) {
			if (devices_[i] == id)
				return handles_[i];
		}
		return nullptr;
	}

	std::vector<SDL_JoystickID> devices_;
	std::vector<SDL_Joystick *> handles_;
};

constexpr ControllerButtonCombo SplitCombo { ControllerButton_BUTTON_LEFTSHOULDER, ControllerButton_BUTTON_A };

TEST_F(ControllerDevicePollingTest, SoloAnyDeviceQueryIsUnchanged)
{
	const SDL_JoystickID pad = Attach();
	Register(pad);
	EXPECT_FALSE(IsControllerButtonPressed(ControllerButton_BUTTON_A));
	Hold(pad, SDL_CONTROLLER_BUTTON_A);
	EXPECT_TRUE(IsControllerButtonPressed(ControllerButton_BUTTON_A));
	EXPECT_TRUE(IsControllerButtonPressedOnDevice(pad, ControllerButton_BUTTON_A));
	Hold(pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
	EXPECT_TRUE(IsControllerButtonComboPressed(SplitCombo));
	EXPECT_TRUE(IsControllerButtonComboPressedOnDevice(pad, SplitCombo));
}

// Both registration orders: a press on the earlier-registered pad must not
// hide or answer for the later one (the PR8354 first-match hazard).
TEST_F(ControllerDevicePollingTest, PressIsVisibleOnlyOnItsOwnDeviceInBothOrders)
{
	for (const bool aFirst : { true, false }) {
		SCOPED_TRACE(aFirst ? "A registered first" : "B registered first");
		const SDL_JoystickID a = Attach();
		const SDL_JoystickID b = Attach();
		Register(aFirst ? a : b);
		Register(aFirst ? b : a);

		Hold(a, SDL_CONTROLLER_BUTTON_X);
		EXPECT_TRUE(IsControllerButtonPressedOnDevice(a, ControllerButton_BUTTON_X));
		EXPECT_FALSE(IsControllerButtonPressedOnDevice(b, ControllerButton_BUTTON_X));

		Hold(b, SDL_CONTROLLER_BUTTON_Y);
		EXPECT_TRUE(IsControllerButtonPressedOnDevice(b, ControllerButton_BUTTON_Y));
		EXPECT_FALSE(IsControllerButtonPressedOnDevice(a, ControllerButton_BUTTON_Y));

		// Both hold the same button: each answers for itself.
		Hold(b, SDL_CONTROLLER_BUTTON_X);
		EXPECT_TRUE(IsControllerButtonPressedOnDevice(a, ControllerButton_BUTTON_X));
		EXPECT_TRUE(IsControllerButtonPressedOnDevice(b, ControllerButton_BUTTON_X));
		Hold(a, SDL_CONTROLLER_BUTTON_X, false);
		EXPECT_FALSE(IsControllerButtonPressedOnDevice(a, ControllerButton_BUTTON_X));
		EXPECT_TRUE(IsControllerButtonPressedOnDevice(b, ControllerButton_BUTTON_X));

		Hold(b, SDL_CONTROLLER_BUTTON_X, false);
		Hold(b, SDL_CONTROLLER_BUTTON_Y, false);
		for (const SDL_JoystickID id : { a, b }) {
			GameController::Remove(id);
			Joystick::Remove(id);
		}
	}
}

TEST_F(ControllerDevicePollingTest, ModifierOnOnePadDoesNotComboWithButtonOnAnother)
{
	const SDL_JoystickID a = Attach();
	const SDL_JoystickID b = Attach();
	Register(a);
	Register(b);
	Hold(a, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
	Hold(b, SDL_CONTROLLER_BUTTON_A);

	// The solo query still combines across pads; that is the hazard co-op avoids.
	EXPECT_TRUE(IsControllerButtonComboPressed(SplitCombo));
	EXPECT_FALSE(IsControllerButtonComboPressedOnDevice(a, SplitCombo));
	EXPECT_FALSE(IsControllerButtonComboPressedOnDevice(b, SplitCombo));

	Hold(a, SDL_CONTROLLER_BUTTON_A);
	EXPECT_TRUE(IsControllerButtonComboPressedOnDevice(a, SplitCombo));
	EXPECT_FALSE(IsControllerButtonComboPressedOnDevice(b, SplitCombo));

	// A plain button (no modifier) needs only that button on that pad.
	const ControllerButtonCombo plain { ControllerButton_BUTTON_A };
	EXPECT_TRUE(IsControllerButtonComboPressedOnDevice(b, plain));
}

// The raw joystick registration is consulted exactly as the any-device query
// does for that device, whatever this build's raw button mapping is.
TEST_F(ControllerDevicePollingTest, RawJoystickRegistrationMatchesItsOwnState)
{
	const SDL_JoystickID raw = Attach(SDL_JOYSTICK_TYPE_FLIGHT_STICK);
	const SDL_JoystickID pad = Attach();
	Register(raw, /*gamepad=*/false);
	Register(pad);
	for (int button = 0; button < SDL_CONTROLLER_BUTTON_MAX; ++button)
		Hold(raw, static_cast<SDL_GameControllerButton>(button));

	const Joystick *const rawJoystick = Joystick::Get(raw);
	ASSERT_NE(rawJoystick, nullptr);
	for (int b = ControllerButton_BUTTON_A; b <= ControllerButton_BUTTON_DPAD_RIGHT; ++b) {
		const auto button = static_cast<ControllerButton>(b);
		SCOPED_TRACE(b);
		EXPECT_EQ(IsControllerButtonPressedOnDevice(raw, button), rawJoystick->IsPressed(button));
		EXPECT_FALSE(IsControllerButtonPressedOnDevice(pad, button));
	}
}

TEST_F(ControllerDevicePollingTest, GamepadStaysPolledWhenOnlyItsRawRegistrationIsRemoved)
{
	const SDL_JoystickID pad = Attach();
	Register(pad);
	Joystick::Remove(pad);
	Hold(pad, SDL_CONTROLLER_BUTTON_B);
	EXPECT_TRUE(IsControllerButtonPressedOnDevice(pad, ControllerButton_BUTTON_B));
}

TEST_F(ControllerDevicePollingTest, UnassignedDevicesReportNothing)
{
	const SDL_JoystickID registered = Attach();
	const SDL_JoystickID unregistered = Attach();
	Register(registered);
	Hold(unregistered, SDL_CONTROLLER_BUTTON_A);
	EXPECT_FALSE(IsControllerButtonPressedOnDevice(unregistered, ControllerButton_BUTTON_A));
	EXPECT_FALSE(IsControllerButtonPressedOnDevice(-1, ControllerButton_BUTTON_A));
	EXPECT_FALSE(IsControllerButtonPressed(ControllerButton_BUTTON_A));

	Hold(registered, SDL_CONTROLLER_BUTTON_A);
	EXPECT_FALSE(IsControllerButtonPressedOnDevice(-1, ControllerButton_BUTTON_A));
	EXPECT_FALSE(IsControllerButtonPressedOnDevice(unregistered, ControllerButton_BUTTON_A));
}

TEST_F(ControllerDevicePollingTest, RemovedPadStopsReportingWhileTheOtherKeepsWorking)
{
	const SDL_JoystickID a = Attach();
	const SDL_JoystickID b = Attach();
	Register(a);
	Register(b);
	Hold(a, SDL_CONTROLLER_BUTTON_START);
	Hold(b, SDL_CONTROLLER_BUTTON_START);
	ASSERT_TRUE(IsControllerButtonPressedOnDevice(a, ControllerButton_BUTTON_START));

	// The button is still physically held on the removed pad's virtual device.
	GameController::Remove(a);
	Joystick::Remove(a);
	EXPECT_FALSE(IsControllerButtonPressedOnDevice(a, ControllerButton_BUTTON_START));
	EXPECT_TRUE(IsControllerButtonPressedOnDevice(b, ControllerButton_BUTTON_START));
	Hold(b, SDL_CONTROLLER_BUTTON_START, false);
	EXPECT_FALSE(IsControllerButtonPressedOnDevice(b, ControllerButton_BUTTON_START));
}

TEST_F(ControllerDevicePollingTest, EventDeviceNamesTheSendingPad)
{
	const SDL_JoystickID a = Attach();
	const SDL_JoystickID b = Attach();
	const SDL_JoystickID raw = Attach(SDL_JOYSTICK_TYPE_FLIGHT_STICK);
	Register(b);
	Register(a);
	Register(raw, /*gamepad=*/false);

	SDL_Event event;
	SDL_zero(event);
	event.type = SDL_CONTROLLERBUTTONDOWN;
	event.cbutton.button = SDL_CONTROLLER_BUTTON_A;
	for (const SDL_JoystickID id : { a, b }) {
		event.cbutton.which = id;
		EXPECT_EQ(ControllerEventDevice(event), id);
	}
	event.cbutton.which = raw;
	EXPECT_EQ(ControllerEventDevice(event), -1);

	SDL_zero(event);
	event.type = SDL_JOYBUTTONDOWN;
	event.jbutton.which = raw;
	EXPECT_EQ(ControllerEventDevice(event), raw);
	event.jbutton.which = b;
	EXPECT_EQ(ControllerEventDevice(event), b);

	SDL_zero(event);
	event.type = SDL_CONTROLLERAXISMOTION;
	event.caxis.which = a;
	event.caxis.axis = SDL_CONTROLLER_AXIS_TRIGGERLEFT;
	EXPECT_EQ(ControllerEventDevice(event), a);

	SDL_zero(event);
	event.type = SDL_KEYDOWN;
	event.key.keysym.sym = SDLK_RETURN;
	EXPECT_EQ(ControllerEventDevice(event), -1);

	GameController::Remove(a);
	Joystick::Remove(a);
	SDL_zero(event);
	event.type = SDL_CONTROLLERBUTTONDOWN;
	event.cbutton.which = a;
	EXPECT_EQ(ControllerEventDevice(event), -1);
}

} // namespace
} // namespace devilution
