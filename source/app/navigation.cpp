#include "navigation.h"
#include <chrono>

VPADStatus vpadBuffer[1];
VPADReadError vpadError;

struct KPADWrapper {
    bool connected;
    KPADExtensionType type;
    KPADStatus status;
};

std::array<KPADWrapper, 4> KPADControllers {{{false, WPAD_EXT_CORE, KPADStatus{}}}};

static std::chrono::steady_clock::time_point lastTriggerTime;
static bool canTrigger = true;

// Technical functions

void initializeInputs() {
    VPADInit();
    KPADInit();
    lastTriggerTime = std::chrono::steady_clock::now();
}

static bool isAnyTriggered() {
    if (vpadError == VPAD_READ_SUCCESS && vpadBuffer[0].trigger != 0) return true;
    for (const auto& pad : KPADControllers) {
        if (!pad.connected) continue;
        if (pad.status.trigger != 0) return true;
        if (pad.status.extensionType == KPADExtensionType::WPAD_EXT_CLASSIC || pad.status.extensionType == KPADExtensionType::WPAD_EXT_MPLUS_CLASSIC) {
            if (pad.status.classic.trigger != 0) return true;
        } else if (pad.status.extensionType == KPADExtensionType::WPAD_EXT_PRO_CONTROLLER) {
            if (pad.status.pro.trigger != 0) return true;
        }
    }
    return false;
}

void updateInputs() {
    // Read VPAD (Gamepad) input
    VPADRead(VPADChan::VPAD_CHAN_0, vpadBuffer, 1, &vpadError);
    if (vpadError != VPAD_READ_SUCCESS) {
        vpadBuffer[0].trigger = 0;
        vpadBuffer[0].release = 0;
    }

    // Read WPAD (Pro Controller, Wiimote, Classic) input
    // Loop over each controller channel
    for (uint32_t i=0; i < KPADControllers.size(); i++) {
        // Test if its connected
        if (WPADProbe((WPADChan)i, &KPADControllers[i].type) != 0) {
            KPADControllers[i].connected = false;
            continue;
        }
        
        KPADControllers[i].connected = true;

        // Read the input
        int32_t count = KPADRead((KPADChan)i, &KPADControllers[i].status, 1);
        if (count <= 0) {
            KPADControllers[i].status.trigger = 0;
            KPADControllers[i].status.release = 0;
            if (KPADControllers[i].status.extensionType == KPADExtensionType::WPAD_EXT_CLASSIC || KPADControllers[i].status.extensionType == KPADExtensionType::WPAD_EXT_MPLUS_CLASSIC) {
                KPADControllers[i].status.classic.trigger = 0;
                KPADControllers[i].status.classic.release = 0;
            } else if (KPADControllers[i].status.extensionType == KPADExtensionType::WPAD_EXT_PRO_CONTROLLER) {
                KPADControllers[i].status.pro.trigger = 0;
                KPADControllers[i].status.pro.release = 0;
            }
        }
    }

    auto now = std::chrono::steady_clock::now();
    if (isAnyTriggered()) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTriggerTime).count() < 150) {
            canTrigger = false;
        } else {
            canTrigger = true;
            lastTriggerTime = now;
        }
    } else {
        canTrigger = true;
    }
}

// Navigation Actions

// Check whether the Gamepad is pressing the specified button
bool vpadButtonPressed(VPADButtons button) {
    if (vpadError == VPAD_READ_SUCCESS && canTrigger) {
        if (vpadBuffer[0].trigger & button) return true;
    }
    return false;
}

bool pressedY() {
    if (vpadError == VPAD_READ_SUCCESS && (vpadBuffer[0].trigger & VPAD_BUTTON_Y) && canTrigger) return true;
    for (const auto& pad : KPADControllers) {
        if (!pad.connected) continue;
        if (pad.status.extensionType == KPADExtensionType::WPAD_EXT_CLASSIC || pad.status.extensionType == KPADExtensionType::WPAD_EXT_MPLUS_CLASSIC) {
            if ((pad.status.classic.trigger & WPAD_CLASSIC_BUTTON_Y) && canTrigger) return true;
        }
        else if (pad.status.extensionType == KPADExtensionType::WPAD_EXT_PRO_CONTROLLER) {
            if ((pad.status.pro.trigger & WPAD_PRO_BUTTON_Y) && canTrigger) return true;
        }
    }
    return false;
}

// Check whether any KPAD controller is pressing the specified button
bool kpadButtonPressed(WPADButton button) {
    if (!canTrigger) return false;
    for (const auto& pad : KPADControllers) {
        if (!pad.connected) continue;

        if (pad.status.extensionType == KPADExtensionType::WPAD_EXT_CORE || pad.status.extensionType == KPADExtensionType::WPAD_EXT_NUNCHUK || pad.status.extensionType == KPADExtensionType::WPAD_EXT_MPLUS_NUNCHUK) {
            if (button == WPAD_BUTTON_A) return pad.status.trigger & WPAD_BUTTON_A;
            if (button == WPAD_BUTTON_B) return pad.status.trigger & WPAD_BUTTON_B;
            if (button == WPAD_BUTTON_PLUS) return pad.status.trigger & WPAD_BUTTON_PLUS;
            if (button == WPAD_BUTTON_UP) return pad.status.trigger & WPAD_BUTTON_UP;
            if (button == WPAD_BUTTON_DOWN) return pad.status.trigger & WPAD_BUTTON_DOWN;
            if (button == WPAD_BUTTON_LEFT) return pad.status.trigger & WPAD_BUTTON_LEFT;
            if (button == WPAD_BUTTON_RIGHT) return pad.status.trigger & WPAD_BUTTON_RIGHT;
        }
        else if (pad.status.extensionType == KPADExtensionType::WPAD_EXT_CLASSIC || pad.status.extensionType == KPADExtensionType::WPAD_EXT_MPLUS_CLASSIC) {
            if (button == WPAD_BUTTON_A) return pad.status.classic.trigger & WPAD_CLASSIC_BUTTON_A;
            if (button == WPAD_BUTTON_B) return pad.status.classic.trigger & WPAD_CLASSIC_BUTTON_B;
            if (button == WPAD_BUTTON_PLUS) return pad.status.classic.trigger & WPAD_CLASSIC_BUTTON_PLUS;
            if (button == WPAD_BUTTON_UP) return pad.status.classic.trigger & WPAD_CLASSIC_BUTTON_UP;
            if (button == WPAD_BUTTON_DOWN) return pad.status.classic.trigger & WPAD_CLASSIC_BUTTON_DOWN;
            if (button == WPAD_BUTTON_LEFT) return pad.status.classic.trigger & WPAD_CLASSIC_BUTTON_LEFT;
            if (button == WPAD_BUTTON_RIGHT) return pad.status.classic.trigger & WPAD_CLASSIC_BUTTON_RIGHT;
        }
        else if (pad.status.extensionType == KPADExtensionType::WPAD_EXT_PRO_CONTROLLER) {
            if (button == WPAD_BUTTON_A) return pad.status.pro.trigger & WPAD_PRO_BUTTON_A;
            if (button == WPAD_BUTTON_B) return pad.status.pro.trigger & WPAD_PRO_BUTTON_B;
            if (button == WPAD_BUTTON_PLUS) return pad.status.pro.trigger & WPAD_PRO_BUTTON_PLUS;
            if (button == WPAD_BUTTON_UP) return pad.status.pro.trigger & WPAD_PRO_BUTTON_UP;
            if (button == WPAD_BUTTON_DOWN) return pad.status.pro.trigger & WPAD_PRO_BUTTON_DOWN;
            if (button == WPAD_BUTTON_LEFT) return pad.status.pro.trigger & WPAD_PRO_BUTTON_LEFT;
            if (button == WPAD_BUTTON_RIGHT) return pad.status.pro.trigger & WPAD_PRO_BUTTON_RIGHT;
        }
    }
    return false;
}

// Check whether a stick value crosses the threshold
bool getStickDirection(float stickValue, float threshold) {
    return threshold < 0 ? (stickValue < threshold) : (stickValue > threshold);
}

// Checks for all sticks whether 
bool getKPADSticksDirection(bool XAxis, float threshold) {
    for (const auto& pad : KPADControllers) {
        if (!pad.connected) continue;
        if (pad.status.extensionType == KPADExtensionType::WPAD_EXT_NUNCHUK || pad.status.extensionType == KPADExtensionType::WPAD_EXT_MPLUS_NUNCHUK) {
            return getStickDirection(XAxis ? pad.status.nunchuk.stick.x : pad.status.nunchuk.stick.y, threshold);
        }
        if (pad.status.extensionType == KPADExtensionType::WPAD_EXT_CLASSIC || pad.status.extensionType == KPADExtensionType::WPAD_EXT_MPLUS_CLASSIC) {
            return getStickDirection(XAxis ? pad.status.classic.leftStick.x : pad.status.classic.leftStick.y, threshold);
        }
        if (pad.status.extensionType == KPADExtensionType::WPAD_EXT_PRO_CONTROLLER) {
            return getStickDirection(XAxis ? pad.status.pro.leftStick.x : pad.status.pro.leftStick.y, threshold);
        }
    }
    return false;
}

bool navigatedUp() {
    return vpadButtonPressed(VPAD_BUTTON_UP) || kpadButtonPressed(WPAD_BUTTON_UP) || getStickDirection(vpadBuffer[0].leftStick.y, 0.7) || getKPADSticksDirection(false, 0.7);
}

bool navigatedDown() {
    return vpadButtonPressed(VPAD_BUTTON_DOWN) || kpadButtonPressed(WPAD_BUTTON_DOWN) || getStickDirection(vpadBuffer[0].leftStick.y, -0.7) || getKPADSticksDirection(false, -0.7);
}

bool navigatedLeft() {
    return vpadButtonPressed(VPAD_BUTTON_LEFT) || kpadButtonPressed(WPAD_BUTTON_LEFT) || getStickDirection(vpadBuffer[0].leftStick.x, 0.7) || getKPADSticksDirection(true, 0.7);
}

bool navigatedRight() {
    return vpadButtonPressed(VPAD_BUTTON_RIGHT) || kpadButtonPressed(WPAD_BUTTON_RIGHT) || getStickDirection(vpadBuffer[0].leftStick.x, -0.7) || getKPADSticksDirection(true, -0.7);
}

// Button Actions
bool pressedOk() {
    return vpadButtonPressed(VPAD_BUTTON_A) || kpadButtonPressed(WPAD_BUTTON_A);
}
bool pressedStart() {
    return vpadButtonPressed(VPAD_BUTTON_PLUS) || kpadButtonPressed(WPAD_BUTTON_PLUS);
}
bool pressedBack() {
    return vpadButtonPressed(VPAD_BUTTON_B) || kpadButtonPressed(WPAD_BUTTON_B);
}

bool pressedX() {
    if (vpadError == VPAD_READ_SUCCESS && (vpadBuffer[0].trigger & VPAD_BUTTON_X) && canTrigger) return true;
    for (const auto& pad : KPADControllers) {
        if (!pad.connected) continue;
        if (pad.status.extensionType == KPADExtensionType::WPAD_EXT_CLASSIC || pad.status.extensionType == KPADExtensionType::WPAD_EXT_MPLUS_CLASSIC) {
            if ((pad.status.classic.trigger & WPAD_CLASSIC_BUTTON_X) && canTrigger) return true;
        }
        else if (pad.status.extensionType == KPADExtensionType::WPAD_EXT_PRO_CONTROLLER) {
            if ((pad.status.pro.trigger & WPAD_PRO_BUTTON_X) && canTrigger) return true;
        }
    }
    return false;
}
