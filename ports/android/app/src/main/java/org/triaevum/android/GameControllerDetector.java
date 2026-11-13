package org.triaevum.android;

import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;

import java.util.Locale;

/**
 * Utility for detecting real physical game controllers (gamepads and joysticks),
 * filtering out false positives caused by internal smartphone sensors (accelerometers, gyroscopes),
 * touchscreens, stylus digitizers, virtual input nodes, and standard keyboards.
 */
public final class GameControllerDetector {

    private static final int[] GAMEPAD_BUTTONS = new int[] {
        KeyEvent.KEYCODE_BUTTON_A,
        KeyEvent.KEYCODE_BUTTON_B,
        KeyEvent.KEYCODE_BUTTON_C,
        KeyEvent.KEYCODE_BUTTON_X,
        KeyEvent.KEYCODE_BUTTON_Y,
        KeyEvent.KEYCODE_BUTTON_Z,
        KeyEvent.KEYCODE_BUTTON_L1,
        KeyEvent.KEYCODE_BUTTON_R1,
        KeyEvent.KEYCODE_BUTTON_L2,
        KeyEvent.KEYCODE_BUTTON_R2,
        KeyEvent.KEYCODE_BUTTON_THUMBL,
        KeyEvent.KEYCODE_BUTTON_THUMBR,
        KeyEvent.KEYCODE_BUTTON_START,
        KeyEvent.KEYCODE_BUTTON_SELECT,
        KeyEvent.KEYCODE_BUTTON_MODE,
        KeyEvent.KEYCODE_BUTTON_1,
        KeyEvent.KEYCODE_BUTTON_2,
        KeyEvent.KEYCODE_BUTTON_3,
        KeyEvent.KEYCODE_BUTTON_4
    };

    private GameControllerDetector() {}

    /**
     * Checks if any genuine physical game controller is currently connected to the system.
     */
    public static boolean hasPhysicalGamepadConnected() {
        int[] deviceIds = InputDevice.getDeviceIds();
        if (deviceIds == null) {
            return false;
        }
        for (int id : deviceIds) {
            InputDevice dev = InputDevice.getDevice(id);
            if (isGameController(dev)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Inspects an InputDevice to verify if it is an actual physical game controller.
     */
    public static boolean isGameController(InputDevice dev) {
        if (dev == null) {
            return false;
        }
        if (dev.isVirtual()) {
            return false;
        }

        int sources = dev.getSources();
        boolean hasGamepadSource = (sources & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD;
        boolean hasJoystickSource = (sources & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK;
        if (!hasGamepadSource && !hasJoystickSource) {
            return false;
        }

        int keyboardType = dev.getKeyboardType();
        boolean isExternal = dev.isExternal();
        String name = dev.getName();

        boolean hasAnyGamepadKey = false;
        try {
            boolean[] hasKeys = dev.hasKeys(GAMEPAD_BUTTONS);
            if (hasKeys != null) {
                for (boolean b : hasKeys) {
                    if (b) {
                        hasAnyGamepadKey = true;
                        break;
                    }
                }
            }
        } catch (Throwable ignored) {}

        boolean hasJoystickAxes = false;
        if (hasJoystickSource) {
            try {
                boolean hasX = dev.getMotionRange(MotionEvent.AXIS_X, InputDevice.SOURCE_JOYSTICK) != null;
                boolean hasY = dev.getMotionRange(MotionEvent.AXIS_Y, InputDevice.SOURCE_JOYSTICK) != null;
                hasJoystickAxes = hasX && hasY;
            } catch (Throwable ignored) {}
        }

        return evaluateDevice(name, isExternal, sources, keyboardType, hasAnyGamepadKey, hasJoystickAxes);
    }

    /**
     * Core decision logic isolated for direct unit testing.
     */
    static boolean evaluateDevice(
            String name,
            boolean isExternal,
            int sources,
            int keyboardType,
            boolean hasAnyGamepadKey,
            boolean hasJoystickAxes) {

        // Exclude standard QWERTY keyboards
        if (keyboardType == InputDevice.KEYBOARD_TYPE_ALPHABETIC) {
            return false;
        }

        // Filter out known non-gamepad hardware keywords (sensors, touch panels, gpio keys, etc.)
        if (name != null) {
            String lower = name.toLowerCase(Locale.ROOT);
            if (lower.contains("accelerometer") ||
                lower.contains("sensor") ||
                lower.contains("gyro") ||
                lower.contains("touchscreen") ||
                lower.contains("touch_panel") ||
                lower.contains("touch") ||
                lower.contains("synaptics") ||
                lower.contains("goodix") ||
                lower.contains("focaltech") ||
                lower.contains("fts") ||
                lower.contains("gpio-keys") ||
                lower.contains("qpnp_pon") ||
                lower.contains("power") ||
                lower.contains("volume") ||
                lower.contains("headset") ||
                lower.contains("earphone") ||
                lower.contains("fingerprint") ||
                lower.contains("hall") ||
                lower.contains("stylus") ||
                lower.contains("pen") ||
                lower.contains("compass") ||
                lower.contains("uinput-") ||
                lower.contains("virtual-search")) {
                return false;
            }
        }

        // 1. If it has physical gamepad buttons, it's a real gamepad
        // (works for external Bluetooth/USB gamepads AND internal gamepads on handheld consoles like Odin/Retroid)
        if (hasAnyGamepadKey) {
            return true;
        }

        // 2. If it is an external peripheral (USB/Bluetooth) with joystick axes (e.g. flight stick, steering wheel)
        boolean hasJoystickSource = (sources & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK;
        if (isExternal && hasJoystickSource && hasJoystickAxes) {
            return true;
        }

        return false;
    }
}

