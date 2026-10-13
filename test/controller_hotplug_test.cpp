#include <gtest/gtest.h>

#include <SDL.h>

#include <vector>

#include "controls/controller.h"
#include "controls/devices/game_controller.h"
#include "controls/devices/joystick.h"

namespace devilution {
namespace {

class ControllerHotplugTest : public testing::Test {
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
		SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
	}

	int Attach(SDL_JoystickType type = SDL_JOYSTICK_TYPE_GAMECONTROLLER)
	{
		const int index = SDL_JoystickAttachVirtual(type, SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 1);
		if (index >= 0)
			devices_.push_back(SDL_JoystickGetDeviceInstanceID(index));
		return index;
	}

	static int DeviceIndex(SDL_JoystickID id)
	{
		for (int index = 0; index < SDL_NumJoysticks(); ++index) {
			if (SDL_JoystickGetDeviceInstanceID(index) == id)
				return index;
		}
		return -1;
	}

	void DispatchHotplugEvents()
	{
		SDL_Event event;
		while (SDL_PollEvent(&event) != 0) {
			// Ignore physical devices on a developer's machine.
			SDL_JoystickID id;
			switch (event.type) {
			case SDL_CONTROLLERDEVICEADDED:
				id = SDL_JoystickGetDeviceInstanceID(event.cdevice.which);
				break;
			case SDL_JOYDEVICEADDED:
				id = SDL_JoystickGetDeviceInstanceID(event.jdevice.which);
				break;
			case SDL_CONTROLLERDEVICEREMOVED:
				id = event.cdevice.which;
				break;
			case SDL_JOYDEVICEREMOVED:
				id = event.jdevice.which;
				break;
			default:
				continue;
			}
			for (const SDL_JoystickID virtualId : devices_) {
				if (id == virtualId) {
					EXPECT_TRUE(HandleControllerAddedOrRemovedEvent(event));
					break;
				}
			}
		}
	}

private:
	std::vector<SDL_JoystickID> devices_;
};

TEST_F(ControllerHotplugTest, RemovingGamepadReleasesItsSdlHandle)
{
	const int index = Attach();
	ASSERT_GE(index, 0) << SDL_GetError();
	const SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(index);
	GameController::Add(index);
	ASSERT_NE(GameController::Get(id), nullptr);
	ASSERT_NE(SDL_GameControllerFromInstanceID(id), nullptr);

	GameController::Remove(id);
	EXPECT_EQ(GameController::Get(id), nullptr);
	EXPECT_EQ(SDL_GameControllerFromInstanceID(id), nullptr);
	EXPECT_EQ(SDL_JoystickFromInstanceID(id), nullptr);
	GameController::Remove(id);
	EXPECT_EQ(SDL_GameControllerFromInstanceID(id), nullptr);
}

TEST_F(ControllerHotplugTest, RemovingJoystickReleasesItsSdlHandle)
{
	const int index = Attach(SDL_JOYSTICK_TYPE_FLIGHT_STICK);
	ASSERT_GE(index, 0) << SDL_GetError();
	const SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(index);
	Joystick::Add(index);
	ASSERT_NE(Joystick::Get(id), nullptr);
	ASSERT_NE(SDL_JoystickFromInstanceID(id), nullptr);

	Joystick::Remove(id);
	EXPECT_EQ(Joystick::Get(id), nullptr);
	EXPECT_EQ(SDL_JoystickFromInstanceID(id), nullptr);
	Joystick::Remove(id);
	EXPECT_EQ(SDL_JoystickFromInstanceID(id), nullptr);
}

TEST_F(ControllerHotplugTest, GamepadRemovalPreservesRawJoystickRegistration)
{
	const int index = Attach();
	ASSERT_GE(index, 0) << SDL_GetError();
	const SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(index);
	GameController::Add(index);
	Joystick::Add(index);
	ASSERT_NE(GameController::Get(id), nullptr);
	ASSERT_NE(Joystick::Get(id), nullptr);

	GameController::Remove(id);
	EXPECT_EQ(SDL_GameControllerFromInstanceID(id), nullptr);
	EXPECT_NE(Joystick::Get(id), nullptr);
	ASSERT_NE(SDL_JoystickFromInstanceID(id), nullptr);
	EXPECT_EQ(SDL_JoystickGetAttached(SDL_JoystickFromInstanceID(id)), SDL_TRUE);
	GameController::Remove(id);
	EXPECT_NE(SDL_JoystickFromInstanceID(id), nullptr);

	Joystick::Remove(id);
	EXPECT_EQ(SDL_JoystickFromInstanceID(id), nullptr);
}

TEST_F(ControllerHotplugTest, JoystickRemovalPreservesGamepadRegistration)
{
	const int index = Attach();
	ASSERT_GE(index, 0) << SDL_GetError();
	const SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(index);
	GameController::Add(index);
	Joystick::Add(index);
	ASSERT_NE(GameController::Get(id), nullptr);
	ASSERT_NE(Joystick::Get(id), nullptr);

	Joystick::Remove(id);
	EXPECT_EQ(Joystick::Get(id), nullptr);
	ASSERT_NE(SDL_GameControllerFromInstanceID(id), nullptr);
	EXPECT_EQ(SDL_GameControllerGetAttached(SDL_GameControllerFromInstanceID(id)), SDL_TRUE);
	Joystick::Remove(id);
	EXPECT_NE(SDL_GameControllerFromInstanceID(id), nullptr);

	GameController::Remove(id);
	EXPECT_EQ(SDL_GameControllerFromInstanceID(id), nullptr);
	EXPECT_EQ(SDL_JoystickFromInstanceID(id), nullptr);
}

TEST_F(ControllerHotplugTest, RemovalPreservesExternalSdlReferences)
{
	const int index = Attach();
	ASSERT_GE(index, 0) << SDL_GetError();
	const SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(index);
	GameController::Add(index);
	Joystick::Add(index);
	SDL_GameController *const externalController = SDL_GameControllerOpen(index);
	ASSERT_NE(externalController, nullptr);
	SDL_Joystick *const externalJoystick = SDL_JoystickOpen(index);
	ASSERT_NE(externalJoystick, nullptr);

	GameController::Remove(id);
	Joystick::Remove(id);
	EXPECT_EQ(SDL_GameControllerFromInstanceID(id), externalController);
	EXPECT_EQ(SDL_JoystickFromInstanceID(id), externalJoystick);
	EXPECT_EQ(SDL_GameControllerGetAttached(externalController), SDL_TRUE);
	EXPECT_EQ(SDL_JoystickGetAttached(externalJoystick), SDL_TRUE);
	GameController::Remove(id);
	Joystick::Remove(id);

	SDL_GameControllerClose(externalController);
	EXPECT_EQ(SDL_GameControllerFromInstanceID(id), nullptr);
	EXPECT_EQ(SDL_JoystickFromInstanceID(id), externalJoystick);
	SDL_JoystickClose(externalJoystick);
	EXPECT_EQ(SDL_JoystickFromInstanceID(id), nullptr);
}

TEST_F(ControllerHotplugTest, RepeatedRealHotplugPreservesTheOtherDevice)
{
	const int survivorIndex = Attach();
	ASSERT_GE(survivorIndex, 0) << SDL_GetError();
	const SDL_JoystickID survivor = SDL_JoystickGetDeviceInstanceID(survivorIndex);
	DispatchHotplugEvents();
	ASSERT_NE(GameController::Get(survivor), nullptr);
	ASSERT_NE(Joystick::Get(survivor), nullptr);

	SDL_JoystickID previous = survivor;
	for (int cycle = 0; cycle < 12; ++cycle) {
		SCOPED_TRACE(cycle);
		const int index = Attach();
		ASSERT_GE(index, 0) << SDL_GetError();
		const SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(index);
		EXPECT_NE(id, previous);
		DispatchHotplugEvents();
		ASSERT_NE(GameController::Get(id), nullptr);
		ASSERT_NE(Joystick::Get(id), nullptr);
		EXPECT_EQ(GameController::All().size(), 2);
		EXPECT_EQ(Joystick::All().size(), 2);
		ASSERT_EQ(SDL_JoystickDetachVirtual(DeviceIndex(id)), 0) << SDL_GetError();
		DispatchHotplugEvents();
		EXPECT_EQ(GameController::Get(id), nullptr);
		EXPECT_EQ(Joystick::Get(id), nullptr);
		EXPECT_EQ(SDL_GameControllerFromInstanceID(id), nullptr);
		EXPECT_EQ(SDL_JoystickFromInstanceID(id), nullptr);
		EXPECT_EQ(GameController::All().size(), 1);
		EXPECT_EQ(Joystick::All().size(), 1);
		EXPECT_NE(GameController::Get(survivor), nullptr);
		EXPECT_NE(Joystick::Get(survivor), nullptr);
		ASSERT_NE(SDL_GameControllerFromInstanceID(survivor), nullptr);
		EXPECT_EQ(SDL_GameControllerGetAttached(SDL_GameControllerFromInstanceID(survivor)), SDL_TRUE);
		previous = id;
	}
}

} // namespace
} // namespace devilution
