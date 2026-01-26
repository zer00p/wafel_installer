#include "cfw.h"
#include "gui.h"
#include "menu.h"
#include "navigation.h"

#include <mocha/mocha.h>

CFWVersion currCFWVersion = CFWVersion::NONE;

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

    WHBLogPrint("Detecting prior iosuhax version...");
    WHBLogFreetypeDraw();
    IOSHandle stroopwafelHandle = IOS_Open("/dev/stroopwafel", (IOSOpenMode)0);
    if (stroopwafelHandle >= IOS_ERROR_OK) {
        WHBLogPrint("Detected stroopwafel...");
        WHBLogFreetypeDraw();
        int res = IOS_Ioctl(stroopwafelHandle, 12345, (void *)nullptr, 0, nullptr, 0);
        WHBLogPrintf("stroopwafel IOCTL returned %d", res);
        WHBLogFreetypeDraw();
        IOS_Close(stroopwafelHandle);

    }


    WHBLogPrint("Detecting prior iosuhax version...");
    WHBLogFreetypeDraw();

    if (IS_CEMU_PRESENT()) {
        WHBLogPrint("Detected that Cemu is being used to run Dumpling...");
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
            WHBLogPrint("Detected previous Dumpling CFW...");
            WHBLogPrint("Attempt to replace it with Dumpling CFW...");
            WHBLogFreetypeDraw();
            currCFWVersion = CFWVersion::DUMPLING;
        }
        else if (mochaVersion == 999) {
            WHBLogPrintf("Detected custom Mocha payload...");
            WHBLogPrintf("Running in Dumpling environment, all devices allowed");
            currCFWVersion = CFWVersion::CUSTOM_MOCHA;
        }
        else {
            currCFWVersion = CFWVersion::MOCHA_FSCLIENT;
        }
        return currCFWVersion;
    }
    else if (ret == MOCHA_RESULT_UNSUPPORTED_API_VERSION) {
        uint8_t forceTiramisu = showDialogPrompt(L"Using an outdated Tiramisu version\nwithout FS client support!\n\nPlease update Tiramisu with this guide:\nhttps://wiiu.hacks.guide/#/tiramisu/sd-preparation\n\nForcing internal CFW will temporarily stop Tiramisu!", L"Exit Dumpling To Update (Recommended)", L"Force Internal CFW And Continue");
        if (forceTiramisu == 1) {
            if (stopMochaServer()) {
                WHBLogFreetypeClear();
                WHBLogPrint("Detected and stopped Tiramisu...");
                WHBLogPrint("Attempt to replace it with Dumpling CFW...");
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
                WHBLogPrint("Exiting Dumpling in 10 seconds...");
                WHBLogFreetypeDraw();
                sleep_for(10s);
                currCFWVersion = CFWVersion::FAILED;
            }
        }
        else {
            WHBLogFreetypeClear();
            WHBLogPrint("Exiting Dumpling...");
            WHBLogPrint("You have to manually update your Tiramisu/Aroma now!");
            WHBLogFreetypeDraw();
            sleep_for(3s);
            currCFWVersion = CFWVersion::FAILED;
        }
        return currCFWVersion;
    }
    WHBLogPrint("Detected no prior (compatible) CFW...");
    WHBLogPrint("Attempt to use internal Dumpling CFW...");
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
        WHBLogPrint("Try restarting your Wii U and launching Dumpling again!");
        return false;
    }
    return true;
}

void shutdownCFW() {
    Mocha_DeInitLibrary();
    sleep_for(1s);
}

CFWVersion getCFWVersion() {
    return currCFWVersion;
}