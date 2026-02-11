#include "cfw.h"
#include "gui.h"
#include "menu.h"
#include "navigation.h"
#include "filesystem.h"

#include <mocha/mocha.h>
#include <stroopwafel/stroopwafel.h> // Assuming this is the correct header

CFWVersion currCFWVersion = CFWVersion::NONE;
bool stroopwafel_available = false;
std::string stroopwafel_plugin_posix_path = "";
bool pending_shutdown = false;

bool stopMochaServer() {
    WHBLogFreetypeClear();
    WHBLogPrint("Opening iosuhax to send stop command...");
    WHBLogFreetypeDraw();

    IOSHandle iosuhaxHandle = IOS_Open("/dev/iosuhax", (IOSOpenMode)0);
    if (iosuhaxHandle < IOS_ERROR_OK) {
        WHBLogPrint("Couldn't open /dev/iosuhax to stop Mocha?");
        WHBLogFreetypeDraw();
        return false;
    }

    WHBLogPrint("Sending stop command to Mocha... ");
    WHBLogFreetypeDraw();
    sleep_for(250ms);

    alignas(0x20) int32_t responseBuffer[0x20 >> 2];
    *responseBuffer = 0;
    IOS_Ioctl(iosuhaxHandle, 0x03/*IOCTL_KILL_SERVER*/, nullptr, 0, responseBuffer, 4);
    
    WHBLogPrint("Waiting for Mocha to stop... ");
    WHBLogFreetypeDraw();
    sleep_for(2s);
    return true;
}

CFWVersion testCFW() {
    // Check for /dev/stroopwafel and initialize libstroopwafel
    WHBLogPrint("Attempting to initialize libstroopwafel...");
    if (Stroopwafel_InitLibrary() == STROOPWAFEL_RESULT_SUCCESS) {
        stroopwafel_available = true;
        WHBLogPrint("libstroopwafel initialized successfully.");

        StroopwafelMinutePath currentPath = {0};
        if (Stroopwafel_GetPluginPath(&currentPath) == STROOPWAFEL_RESULT_SUCCESS) {
            std::string fullVolPath;
            if (currentPath.device == STROOPWAFEL_MIN_DEV_SLC) {
                fullVolPath = "/vol/storage_slc" + std::string(currentPath.path);
            } else if (currentPath.device == STROOPWAFEL_MIN_DEV_SD) {
                fullVolPath = "/vol/external01" + std::string(currentPath.path);
            }
            if (!fullVolPath.empty()) {
                stroopwafel_plugin_posix_path = convertToPosixPath(fullVolPath.c_str());
            }
        }
    } else {
        WHBLogPrint("libstroopwafel initialization failed.");
    }


    WHBLogPrint("Detecting prior iosuhax version...");
    WHBLogFreetypeDraw();

    if (IS_CEMU_PRESENT()) {
        WHBLogPrint("Detected that Cemu is being used to run ISFShax Loader...");
        WHBLogPrint("Skip exploits since they aren't required.");
        WHBLogFreetypeDraw();
        sleep_for(2s);
        currCFWVersion = CFWVersion::CEMU;
        return currCFWVersion;
    }

    uint32_t mochaVersion = 0;
    MochaUtilsStatus ret = Mocha_CheckAPIVersion(&mochaVersion);
    if (ret == MOCHA_RESULT_SUCCESS) {
        if (mochaVersion == (1 + 1337)) {
            WHBLogPrint("Detected previous ISFShax Loader CFW...");
            WHBLogPrint("Attempt to replace it with ISFShax Loader CFW...");
            WHBLogFreetypeDraw();
            currCFWVersion = CFWVersion::DUMPLING;
        }
        else if (mochaVersion == 999) {
            WHBLogPrintf("Detected custom Mocha payload...");
            WHBLogPrintf("Running in ISFShax Loader environment, all devices allowed");
            currCFWVersion = CFWVersion::CUSTOM_MOCHA;
        }
        else {
            currCFWVersion = CFWVersion::MOCHA_FSCLIENT;
        }
        return currCFWVersion;
    }
    else if (ret == MOCHA_RESULT_UNSUPPORTED_API_VERSION) {
        uint8_t forceTiramisu = showDialogPrompt(L"Using an outdated Tiramisu version\nwithout FS client support!\n\nPlease update Tiramisu with this guide:\nhttps://wiiu.hacks.guide/#/tiramisu/sd-preparation\n\nForcing internal CFW will temporarily stop Tiramisu!", L"Exit ISFShax Loader To Update (Recommended)", L"Force Internal CFW And Continue");
        if (forceTiramisu == 1) {
            if (stopMochaServer()) {
                WHBLogFreetypeClear();
                WHBLogPrint("Detected and stopped Tiramisu...");
                WHBLogPrint("Attempt to replace it with ISFShax Loader CFW...");
                WHBLogFreetypeDraw();
                currCFWVersion = CFWVersion::NONE;
            }
            else {
                WHBLogFreetypeClear();
                WHBLogPrint("Failed to stop Tiramisu CFW!");
                WHBLogPrint("");
                WHBLogPrint("Please update Tiramisu with this guide:");
                WHBLogPrint("https://wiiu.hacks.guide/#/tiramisu/sd-preparation");
                WHBLogPrint("since stopping CFW isn't working properly");
                WHBLogPrint("");
                WHBLogPrint("Exiting ISFShax Loader in 10 seconds...");
                WHBLogFreetypeDraw();
                sleep_for(10s);
                currCFWVersion = CFWVersion::FAILED;
            }
        }
        else {
            WHBLogFreetypeClear();
            WHBLogPrint("Exiting ISFShax Loader...");
            WHBLogPrint("You have to manually update your Tiramisu/Aroma now!");
            WHBLogFreetypeDraw();
            sleep_for(3s);
            currCFWVersion = CFWVersion::FAILED;
        }
        return currCFWVersion;
    }
    WHBLogPrint("Detected no prior (compatible) CFW...");
    WHBLogPrint("Attempt to use internal ISFShax Loader CFW...");
    WHBLogFreetypeDraw();
    currCFWVersion = CFWVersion::NONE;
    return currCFWVersion;
}

bool initCFW() {
    WHBLogPrint("Preparing iosuhax...");
    WHBLogFreetypeDraw();

    if (getCFWVersion() == CFWVersion::CEMU) return true;

    // Connect to iosuhax
    if (Mocha_InitLibrary() != MOCHA_RESULT_SUCCESS) {
        WHBLogPrint("Couldn't open /dev/iosuhax :/");
        WHBLogPrint("Something interfered with the exploit...");
        WHBLogPrint("Try restarting your Wii U and launching ISFShax Loader again!");
        return false;
    }
    return true;
}

void shutdownCFW() {
    Mocha_DeInitLibrary();
    if (stroopwafel_available) {
        Stroopwafel_DeInitLibrary();
    }
    sleep_for(1s);
}

CFWVersion getCFWVersion() {
    return currCFWVersion;
}

bool isStroopwafelAvailable() {
    return stroopwafel_available;
}

bool isIsfshaxInstalled() {
    uint32_t val = 0;
    if (Mocha_IOSUKernelRead32(0x1072272C, &val) != MOCHA_RESULT_SUCCESS) {
        return false;
    }
    return val != 0xe3e05000;
}

bool isSdEmulated() {
    uint32_t val = 0;
    if (Mocha_IOSUKernelRead32(0x1077eda0, &val) != MOCHA_RESULT_SUCCESS) {
        return false;
    }
    return val == 0xe3a03006;
}

std::string getStroopwafelPluginPosixPath() {
    return stroopwafel_plugin_posix_path;
}

void setStroopwafelPluginPosixPath(const std::string& path) {
    stroopwafel_plugin_posix_path = path;
}

bool isShutdownPending() {
    return pending_shutdown;
}

void setShutdownPending(bool pending) {
    pending_shutdown = pending;
}