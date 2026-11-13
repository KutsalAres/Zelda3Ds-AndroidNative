package org.triaevum.android;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.view.InputDevice;

import org.junit.Test;

public final class GameControllerDetectorTest {

    @Test
    public void rejectsNullAndVirtualDevices() {
        assertFalse(GameControllerDetector.isGameController(null));
    }

    @Test
    public void rejectsInternalPhoneSensors() {
        // Accelerometer
        assertFalse(GameControllerDetector.evaluateDevice(
            "bmi160_accel",
            false, // isExternal
            InputDevice.SOURCE_JOYSTICK,
            InputDevice.KEYBOARD_TYPE_NONE,
            false, // hasAnyGamepadKey
            true   // hasJoystickAxes
        ));

        // Gyroscope
        assertFalse(GameControllerDetector.evaluateDevice(
            "lsm6dso_gyro",
            false,
            InputDevice.SOURCE_JOYSTICK,
            InputDevice.KEYBOARD_TYPE_NONE,
            false,
            true
        ));

        // Generic sensor name
        assertFalse(GameControllerDetector.evaluateDevice(
            "qualcomm,sensor",
            false,
            InputDevice.SOURCE_JOYSTICK,
            InputDevice.KEYBOARD_TYPE_NONE,
            false,
            true
        ));
    }

    @Test
    public void rejectsTouchscreensAndKeyboards() {
        // Touchscreen
        assertFalse(GameControllerDetector.evaluateDevice(
            "fts_ts",
            false,
            InputDevice.SOURCE_TOUCHSCREEN,
            InputDevice.KEYBOARD_TYPE_NONE,
            false,
            false
        ));

        // QWERTY keyboard with alphabetic keys
        assertFalse(GameControllerDetector.evaluateDevice(
            "Physical Keyboard",
            true,
            InputDevice.SOURCE_KEYBOARD,
            InputDevice.KEYBOARD_TYPE_ALPHABETIC,
            false,
            false
        ));
    }

    @Test
    public void acceptsExternalBluetoothGamepad() {
        assertTrue(GameControllerDetector.evaluateDevice(
            "Xbox Wireless Controller",
            true, // isExternal
            InputDevice.SOURCE_GAMEPAD | InputDevice.SOURCE_JOYSTICK,
            InputDevice.KEYBOARD_TYPE_NON_ALPHABETIC,
            true, // hasAnyGamepadKey
            true  // hasJoystickAxes
        ));

        assertTrue(GameControllerDetector.evaluateDevice(
            "Wireless Controller", // DualShock 4 / DualSense
            true,
            InputDevice.SOURCE_GAMEPAD | InputDevice.SOURCE_JOYSTICK,
            InputDevice.KEYBOARD_TYPE_NON_ALPHABETIC,
            true,
            true
        ));
    }

    @Test
    public void acceptsExternalUsbGamepad() {
        assertTrue(GameControllerDetector.evaluateDevice(
            "Razer Kishi",
            true,
            InputDevice.SOURCE_GAMEPAD | InputDevice.SOURCE_JOYSTICK,
            InputDevice.KEYBOARD_TYPE_NON_ALPHABETIC,
            true,
            true
        ));
    }

    @Test
    public void acceptsHandheldConsoleBuiltInGamepad() {
        // Dedicated Android gaming handhelds (Ayn Odin, Retroid Pocket) have built-in physical gamepads (isExternal=false)
        assertTrue(GameControllerDetector.evaluateDevice(
            "odin_joypad",
            false, // isExternal
            InputDevice.SOURCE_GAMEPAD | InputDevice.SOURCE_JOYSTICK,
            InputDevice.KEYBOARD_TYPE_NON_ALPHABETIC,
            true,  // hasAnyGamepadKey (A, B, X, Y buttons present)
            true
        ));
    }

    @Test
    public void acceptsExternalJoystickOrFlightStickWithoutAbxy() {
        assertTrue(GameControllerDetector.evaluateDevice(
            "Generic Flight Stick",
            true, // isExternal
            InputDevice.SOURCE_JOYSTICK,
            InputDevice.KEYBOARD_TYPE_NON_ALPHABETIC,
            false, // no standard ABXY
            true   // has X and Y axes
        ));
    }
}

