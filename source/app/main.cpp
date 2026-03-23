#include "menu.h"
#include "partition_manager.h"
#include "startup_checks.h"
#include "navigation.h"
#include "cfw.h"
#include "filesystem.h"
#include "exploit.h"
#include "gui.h"
#include <unistd.h> // For access function
#include <chrono>
#include <thread>

// Initialize correct heaps for CustomRPXLoader
extern "C" void __init_wut_malloc();
extern "C" [[maybe_unused]] void __preinit_user(MEMHeapHandle *outMem1, MEMHeapHandle *outFG, MEMHeapHandle *outMem2) {
    __init_wut_malloc();
}

int main() {
    // Initialize libraries
    initializeGUI();
    WHBLogCafeInit();
    FSInit();
    FSAInit();
    nn::act::Initialize();
    ACPInitialize();
    initializeInputs();



    IMDisableAPD(); // Disable auto-shutdown feature

    // Start Wafel Installer
    showLoadingScreen();
    if (testCFW() != FAILED && ((getCFWVersion() == MOCHA_FSCLIENT || getCFWVersion() == CEMU || getCFWVersion() == CUSTOM_MOCHA) || installCFW()) && initCFW() ) {
        setupMountGuard(getCFWVersion());
        mountSystemDrives();
        WHBLogFreetypePrint(L" ");
        WHBLogPrint("Finished loading!");
        WHBLogFreetypeDraw();
        sleep_for(2s);
        if (performStartupChecks()) {
            showMainMenu();
        }
    }

    bool reboot = getCFWVersion() != MOCHA_FSCLIENT  || isRebootPending();

    if (isShutdownPending()) {
        WHBLogFreetypeStartScreen();
        WHBLogPrint("Shutting down now...");
        WHBLogFreetypeDraw();
        sleep_for(3s);
        OSShutdown();
    } else if (isRebootPending()) {
        WHBLogFreetypeStartScreen();
        WHBLogPrint("Rebooting now...");
        WHBLogPrint("To apply the changes.");
        WHBLogFreetypeDraw();
        sleep_for(3s);
    }
    else {
        WHBLogPrint("");
        WHBLogPrint(!reboot ? "Exiting Wafel Installer..." : "Exiting Wafel Installer and rebooting Wii U...");
        WHBLogFreetypeDraw();
        sleep_for(5s);
    }

    // Close application properly
    unmountSystemDrives();
    shutdownCFW();
    ACPFinalize();
    nn::act::Finalize();
    FSShutdown();
    shutdownInputs();
    shutdownGUI();

    exitApplication(reboot, isFullRebootPending());
}