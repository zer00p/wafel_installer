#include "uninstall.h"
#include "menu.h"
#include "gui.h"
#include "nand_check.h"
#include "common_paths.h"
#include "pluginmanager.h"
#include "filesystem.h"
#include "cfw.h"
#include "isfshax_menu.h"
#include "partition_manager.h"
#include <whb/sdcard.h>
#include <mocha/fsa.h>
#include <chrono>

using namespace std::chrono_literals;

bool uninstallChecks() {
    if (isRedNAND()) {
        showDialogPrompt(L"redNAND is detected.\nStroopwafel and ISFShax are required for redNAND.\nUninstallation is not possible.", L"OK");
        return false;
    }

    if (fileExist(Paths::SdPluginsDir + "/4usbmlc.ipx") || fileExist(Paths::SlcPluginsDir + "/4usbmlc.ipx")) {
        showDialogPrompt(L"USBMLC is detected.\nStroopwafel and ISFShax are required for USBMLC.\nUninstallation is not possible.", L"OK");
        return false;
    }

    if (hasUnknownPlugins()) {
        return false;
    }

    if (!runSystemIntegrityCheck()) {
        return false;
    }

    return true;
}

void uninstallStroopwafelMenu(bool showWarning) {
    if (showWarning) {
        if (!uninstallChecks()) {
            return;
        }
    }

    if (showWarning) {

        const wchar_t* warningMessage =
            L"Please read carefully:\n \n"
            L"This will remove Stroopwafel from your console.\n"
            L"Modifications made by other tools might still persist.\n \n"
            L"IMPORTANT: If you use redNAND, do NOT proceed.\n"
            L"If you installed custom keyboards or themes, you MUST\n"
            L"undo these changes BEFORE uninstalling. Removing\n"
            L"Stroopwafel otherwise might cause a BRICK.\n";

        if (showDialogPrompt(warningMessage, L"I have undone everything, continue", L"Cancel", nullptr, nullptr, 1) != 0) {
            return;
        }
    }

    // Stroopwafel removal
    bool stroopOnSlc = dirExist(Paths::SlcPluginsDir) || fileExist(Paths::SlcFwImg);
    bool stroopOnSd = dirExist(Paths::SdPluginsDir) || fileExist(Paths::SdFwImg) || dirExist(Paths::SdMinuteDir);

    uint8_t stroopChoice = 255;
    if (stroopOnSlc && stroopOnSd) {
        stroopChoice = showDialogPrompt(L"Stroopwafel files found on both SLC and SD card.\nWhere do you want to remove them from?", L"Both", L"SLC only", L"SD card only", L"Skip");
    } else if (stroopOnSlc) {
        if (showDialogPrompt(L"Stroopwafel files found on SLC.\nDo you want to remove them?", L"Yes", L"No") == 0) {
            stroopChoice = 1; // SLC only
        }
    } else if (stroopOnSd) {
        if (showDialogPrompt(L"Stroopwafel files found on SD card.\nDo you want to remove them?", L"Yes", L"No") == 0) {
            stroopChoice = 2; // SD only
        }
    }

    if (stroopChoice != 255 && stroopChoice != 3) {
        WHBLogFreetypeStartScreen();
        WHBLogFreetypePrint(L"Removing Stroopwafel files...");
        WHBLogFreetypeDraw();
        setFullRebootPending(true);

        // Remove from SLC
        if (stroopChoice == 0 || stroopChoice == 1) {
            if (checkSystemAccess()) {
                WHBLogFreetypePrint(L"Removing SLC files...");
                WHBLogFreetypeDraw();
                removeFile(Paths::SlcFwImg);
                deleteDirContent(Paths::SlcPluginsDir);
                removeDir(Paths::SlcPluginsDir);
                if (isDirEmpty(Paths::SlcHaxDir)) {
                     removeDir(Paths::SlcHaxDir);
                }
            }
        }
        // Remove from SD
        if (stroopChoice == 0 || stroopChoice == 2) {
            if (WHBMountSdCard() == 1) {
                WHBLogFreetypePrint(L"Removing SD files...");
                WHBLogFreetypeDraw();
                removeFile(Paths::SdFwImg);
                deleteDirContent(Paths::SdPluginsDir);
                removeDir(Paths::SdPluginsDir);
                deleteDirContent(Paths::SdMinuteDir);
                removeDir(Paths::SdMinuteDir);
                WHBUnmountSdCard();
            }
        }
        WHBLogFreetypePrint(L"Done removing Stroopwafel files.");
        WHBLogFreetypeDraw();
        sleep_for(2s);
    }
}

void showUninstallMenu() {
    if (!uninstallChecks()) {
        return;
    }

    const wchar_t* warningMessage =
        L"Please read carefully:\n \n"
        L"Reinstalling ISFShax won't fix issues. Keeping it is recommended.\n \n"
        L"This will undo all modifications this tool made, turning it stock.\n"
        L"It will also offer to reset the SD card if partitioned.\n"
        L"Modifications made by other tools might still persist.\n \n"
        L"IMPORTANT: If you use redNAND, do NOT proceed.\n"
        L"If you installed custom keyboards or themes, you MUST\n"
        L"undo these changes BEFORE uninstalling. Removing\n"
        L"stroopwafel/isfshax otherwise might cause a BRICK.";

    if (showDialogPrompt(warningMessage, L"I have undone everything, continue", L"Cancel", nullptr, nullptr, 1) != 0) {
        return;
    }

    uninstallStroopwafelMenu(false);

    if (checkSystemAccess()) {
        if (!dirExist(Paths::SlcHaxDir)) {
            createDirectories(Paths::SlcHaxDir);
        }
        FILE* f = fileFopen((Paths::SlcHaxDir + "/uninst.mrk").c_str(), "w");
        if (f) {
            fputs("uninstall", f);
            fclose(f);
        }
    }


    showDialogPrompt(L"Stroopwafel has been removed.\nYour console will now reboot. Please launch the Wafel Installer again\nvia wafel.xyz from the browser to complete the uninstallation.", L"OK");
    setRebootPending(true);
    setFullRebootPending(true);
}

void resumeUninstall() {
    // SD Card Check and Format
    if (WHBMountSdCard() == 1) {
        FSAClientHandle fsaHandle = FSAAddClient(nullptr);
        if (fsaHandle >= 0) {
            FSADeviceInfo deviceInfo;
            if ((FSStatus)FSAGetDeviceInfo(fsaHandle, "/dev/sdcard01", &deviceInfo) == FS_STATUS_OK) {
                if (showDialogPrompt(L"Do you want to format the entire SD card to FAT32?\nThis will delete ALL data on it.", L"Yes, format SD", L"No, keep as is") == 0) {
                    if (getCFWVersion() == MOCHA_FSCLIENT) {
                        showDialogPrompt(L"Aroma is running. Formatting the SD card while running\n"
                                         L"Aroma is not possible from here.\n \n"
                                         L"Please come back by using wafel.xyz after uninstalling stroopwafel\n", L"OK");
                    } else {
                        int status = WHBUnmountSdCard();
                        if (status != 1) {
                            WHBLogPrintf("Unmount failed (status: %d), ignoring...", status);
                            WHBLogFreetypeDraw();
                        } else if (!formatWholeDrive(fsaHandle, "/dev/sdcard01", deviceInfo)) {
                            showErrorPrompt(L"Failed to format SD card. Continuing with uninstall...");
                        } else {
                            showSuccessPrompt(L"SD card formatted successfully.");
                        }
                    }
                }
            }
            FSADelClient(fsaHandle);
        }
    }
    WHBUnmountSdCard();

    // ISFShax removal
    if (isIsfshaxInstalled()) {
        const wchar_t* isfshaxMessage =
            L"Do you want to uninstall ISFShax?\n \n"
            L"WARNING: It is STRONGLY recommended to KEEP ISFShax installed.\n"
            L"It provides brick protection\n"
            L"There is usually no reason to uninstall it.";
        if (showDialogPrompt(isfshaxMessage, L"Yes, uninstall ISFShax", L"No, keep ISFShax", nullptr, nullptr, 1) == 0) {
            if (checkSystemAccess()) {
                installIsfshax(true, false); // Uninstall
                // The console will reboot here if successful
            }
        }
    }

    showSuccessPrompt(L"Uninstall process finished!");
}
