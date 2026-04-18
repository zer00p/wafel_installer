#include "menu.h"
#include "isfshax_menu.h"
#include "minute_config.h"
#include "pluginmanager.h"
#include "partition_manager.h"
#include "navigation.h"
#include "filesystem.h"
#include "common_paths.h"
#include "gui.h"
#include "cfw.h"
#include "fw_img_loader.h"
#include "download.h"
#include <isfshax_cmd.h>
#include <vector>
#include <string>
#include <dirent.h>
#include <whb/sdcard.h>
#include <mocha/fsa.h>
#include "fw_img_loader.h"
#include "startup_checks.h"

// Menu screens

void showLoadingScreen() {
    WHBLogFreetypeSetBackgroundColor(0xd4860000);
    WHBLogFreetypeSetFontColor(0xFFFFFFFF);
    WHBLogFreetypeSetFontSize(22);
    WHBLogPrint("Wafel Installer");
    WHBLogPrint("-- Based on dumpling made by Crementif, Emiyl --");
    WHBLogPrint("");
    WHBLogFreetypeDraw();
}

#define OPTION(n) (selectedOption == (n) ? L'>' : L' ')

bool checkSystemAccess(bool suggestExit) {
    if (!isSlcMounted()) {
        if (suggestExit) {
            uint8_t choice = showDialogPrompt(L"Cannot access the SLC!\nPlease make sure to disable 'System Access' in ftpiiu if you have it running.", L"Continue", L"Exit", nullptr, nullptr, 1);
            return (choice == 0);
        } else {
            showDialogPrompt(L"Cannot access the SLC!\nPlease make sure to disable 'System Access' in ftpiiu if you have it running.", L"OK");
            return false;
        }
    }
    return true;
}

void loadArbitraryFwImgMenu() {
    if (!isStroopwafelAvailable()) {
        if (wasStroopwafelDownloadedInSession()) {
            showDialogPrompt(L"Stroopwafel was just installed.\nPlease reboot your console to activate it before using this feature.", L"OK");
        } else {
            uint8_t choice = showDialogPrompt(L"Stroopwafel is required to load arbitrary fw.img files.\nDo you want to install it now?", L"Yes", L"No");
            if (choice == 0) {
                installStroopwafelMenu();
            }
        }
        return;
    }

    if (WHBMountSdCard() != 1) {
        setErrorPrompt(L"Failed to mount SD card!");
        showErrorPrompt(L"OK");
        return;
    }

    std::string cfwDir = Paths::SdCfwDir;
    if (!dirExist(cfwDir)) {
        if (mkdir(cfwDir.c_str(), 0755) != 0 && errno != EEXIST) {
            setErrorPrompt(L"Failed to create wiiu/cfw directory on SD!");
            showErrorPrompt(L"OK");
            return;
        }
        showDialogPrompt(L"The directory 'wiiu/cfw' has been created on your SD card.\nPlease put your fw.img files there to boot them.", L"OK");
        return;
    }

    DIR* dir = opendir(cfwDir.c_str());
    if (!dir) {
        setErrorPrompt(L"Failed to open wiiu/cfw directory!");
        showErrorPrompt(L"OK");
        return;
    }

    std::vector<std::wstring> files;
    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_type != DT_DIR) {
            files.push_back(toWstring(ent->d_name));
        }
    }
    closedir(dir);

    if (files.empty()) {
        showDialogPrompt(L"No files found in 'wiiu/cfw' on SD card.\nPlease put your fw.img files there.", L"OK");
        return;
    }

    uint8_t choice = showDialogPrompt(L"Select a fw.img to load:", files);
    if (choice != 255) {
        std::string fullPath = Paths::SdCfwDir + "/" + toString(files[choice]);
        loadFwImg(fullPath);
    }
}

bool installStroopwafel() {
    bool sdEmulated = isSdEmulated();
    bool toSD = false;

    if (sdEmulated) {
        uint8_t choice = showDialogPrompt(L"Where do you want to download Stroopwafel?\nNote: Stroopwafel cannot be installed to the USB device, even when using SD emulation.", L"SLC", L"Cancel");
        if (choice != 0) return false;
        toSD = false;
    } else {
        uint8_t choice = showDialogPrompt(L"Where do you want to download Stroopwafel?\nSD card is recommended.", L"SD Card", L"SLC", L"Cancel");
        if (choice == 2 || choice == 255) return false;
        toSD = (choice == 0);
    }

    if (!toSD && !checkSystemAccess()) return false;

    if (downloadStroopwafelFiles(toSD)) {
        showSuccessPrompt(L"Stroopwafel files downloaded successfully!");
        return true;
    } else {
        showErrorPrompt(L"OK");
        return false;
    }
}

void installStroopwafelMenu() {
    if (installStroopwafel()) {
        performIsfshaxCheck(false, false);
    }
}

void showMainMenu() {
    uint8_t selectedOption = 0;
    while(true) {
        bool startSelectedOption = false;
        while(!startSelectedOption) {
            CHECK_SHUTDOWN();
            // Print menu text
            WHBLogFreetypeStartScreen();
            WHBLogFreetypePrint(L"Wafel Installer");
            WHBLogFreetypePrint(L"===============================");
            WHBLogFreetypePrintf(L"%C Stroopwafel Plugin Manager", OPTION(0));
            WHBLogFreetypePrintf(L"%C Load custom fw.img", OPTION(1));
            WHBLogFreetypePrintf(L"%C Stroopwafel by shinyquagsire23", OPTION(2));
            WHBLogFreetypePrintf(L"%C ISFShax by rw_r_r_0644", OPTION(3));
            WHBLogFreetypePrintf(L"%C Download Aroma by Maschell", OPTION(4));
            WHBLogFreetypePrintf(L"%C Configure Minute Autoboot", OPTION(5));
            WHBLogFreetypePrintf(L"%C SDUSB", OPTION(6));
            WHBLogFreetypePrintf(L"%C USB Partition", OPTION(7));
            WHBLogFreetypePrintf(L"%C Format and Partition", OPTION(8));
            WHBLogFreetypePrintf(L"%C Guided Uninstall", OPTION(9));
            WHBLogFreetypePrint(L" ");

            WHBLogFreetypeScreenPrintBottom(L"===============================");
            std::wstring bottomStatus = L"CFW: " + std::wstring(getCFWVersionName(getCFWVersion())) + L" | ";
            bottomStatus += L"ISFShax: " + std::wstring(isIsfshaxInstalled() ? L"Installed" : L"Not installed") + L" | ";
            if (!isStroopwafelAvailable()) {
                bottomStatus += L"Stroopwafel: Not active";
            } else {
                std::wstring path = toWstring(getStroopwafelPluginPath());
                if (path.empty()) {
                    bottomStatus += L"Stroopwafel: Active (Path unknown)";
                } else {
                    bottomStatus += L"Plugins: " + path;
                }
            }
            WHBLogFreetypeScreenPrintBottom(bottomStatus.c_str());
            WHBLogFreetypeScreenPrintBottom(L"\uE000 Button = Select Option \uE001 Button = Exit Wafel Installer");
            WHBLogFreetypeScreenPrintBottom(L"");
            WHBLogFreetypeDrawScreen();

            // Loop until there's new input
            updateInputs();
            while(!startSelectedOption) {
                updateInputs();
                // Check each button state
                if (navigatedUp()) {
                    if (selectedOption > 0) {
                        selectedOption--;
                        break;
                    }
                }
                if (navigatedDown()) {
                    if (selectedOption < 9) {
                        selectedOption++;
                        break;
                    }
                }
                if (pressedOk()) {
                    startSelectedOption = true;
                    break;
                }
                if (pressedBack()) {
                    uint8_t exitSelectedOption = showDialogPrompt(getCFWVersion() == MOCHA_FSCLIENT ? L"Do you really want to exit Wafel Installer?" : L"Do you really want to exit Wafel Installer?\nYour console will reboot to prevent compatibility issues!", L"Yes", L"No", nullptr, nullptr, 1);
                    if (exitSelectedOption == 0) {
                        WHBLogFreetypeClear();
                        return;
                    }
                    else break;
                }
            }
        }

        // Go to the selected menu
        switch(selectedOption) {
            case 0:
                showPluginManager();
                break;
            case 1:
                loadArbitraryFwImgMenu();
                break;
            case 2:
                showStroopwafelMenu();
                break;
            case 3:
                installIsfshaxMenu();
                break;
            case 4:
                askAndDownloadAroma();
                break;
            case 5:
                configureMinuteMenu();
                break;
            case 6:
                showSDUSBMenu();
                break;
            case 7:
                showUSBPartitionMenu();
                break;
            case 8:
                formatAndPartitionMenu();
                break;
            case 9:
                showUninstallMenu();
                break;
            default:
                break;
        }
    }
}


// Helper functions

uint8_t showDialogPrompt(const wchar_t* message, const std::vector<std::wstring>& buttons, uint8_t defaultOption, bool clearScreen) {
    CHECK_SHUTDOWN_VAL(255);
    uint8_t selectedOption = defaultOption;
    uint8_t numButtons = buttons.size();
    uint32_t startPos = clearScreen ? 0 : WHBLogFreetypeGetScreenPosition();

    while(true) {
        CHECK_SHUTDOWN_VAL(255);
        if (clearScreen) WHBLogFreetypeStartScreen();
        else WHBLogFreetypeSetScreenPosition(startPos);

        // Print each line
        std::wistringstream messageStream(message);
        std::wstring line;

        while(std::getline(messageStream, line)) {
            WHBLogFreetypePrint(line.c_str());
        }

        WHBLogFreetypePrint(L" ");
        for (uint8_t i = 0; i < numButtons; i++) {
            WHBLogFreetypePrintf(L"%C [%S]", OPTION(i), buttons[i].c_str());
        }
        WHBLogFreetypePrint(L" ");
        WHBLogFreetypeScreenPrintBottom(L"===============================");
        WHBLogFreetypeScreenPrintBottom(L"\u2191/\u2193 = Change Option, \uE000 = Select Option");
        WHBLogFreetypeDrawScreen();

        // Input loop
        updateInputs();
        while (true) {
            CHECK_SHUTDOWN_VAL(255);
            updateInputs();
            // Handle navigation between the buttons
            if (navigatedUp() && selectedOption > 0) {
                selectedOption--;
                break;
            }
            if (navigatedDown() && selectedOption < numButtons - 1) {
                selectedOption++;
                break;
            }

            if (pressedOk()) {
                WHBLogFreetypeStartScreen();
                return selectedOption;
            }

            if (pressedBack()) {
                WHBLogFreetypeStartScreen();
                return 255;
            }
        }
    }
}

uint8_t showDialogPrompt(const wchar_t* message, const wchar_t* button1, const wchar_t* button2, const wchar_t* button3, const wchar_t* button4, uint8_t defaultOption, bool clearScreen) {
    std::vector<std::wstring> buttons;
    buttons.push_back(button1);
    if (button2) buttons.push_back(button2);
    if (button3) buttons.push_back(button3);
    if (button4) buttons.push_back(button4);
    return showDialogPrompt(message, buttons, defaultOption, clearScreen);
}

uint8_t showDialogPrompt(const wchar_t* message, const wchar_t* button1, const wchar_t* button2, const wchar_t* button3, const wchar_t* button4, const wchar_t* button5, const wchar_t* button6, uint8_t defaultOption, bool clearScreen) {
    std::vector<std::wstring> buttons;
    buttons.push_back(button1);
    if (button2) buttons.push_back(button2);
    if (button3) buttons.push_back(button3);
    if (button4) buttons.push_back(button4);
    if (button5) buttons.push_back(button5);
    if (button6) buttons.push_back(button6);
    return showDialogPrompt(message, buttons, defaultOption, clearScreen);
}

void showDialogPrompt(const wchar_t* message, const wchar_t* button, bool clearScreen) {
    showDialogPrompt(message, button, nullptr, nullptr, nullptr, 0, clearScreen);
}

const wchar_t* errorMessage = nullptr;
void setErrorPrompt(const wchar_t* message) {
    errorMessage = message;
}

std::wstring messageCopy;
void setErrorPrompt(std::wstring message) {
    messageCopy = std::move(message);
    setErrorPrompt(messageCopy.c_str());
}

bool showErrorPrompt(const wchar_t* button, bool retryAllowed) {
    WHBLogFreetypeScreenPrintBottom(L"An error occurred! Press A to continue.");
    WHBLogFreetypeDraw();

    // Wait for A
    while(true) {
        updateInputs();
        if (pressedOk()) break;
    }

    std::wstring promptMessage(L"An error occurred:\n");
    if (errorMessage) promptMessage += errorMessage;
    else promptMessage += L"No error was specified!";

    if (retryAllowed) {
        return showDialogPrompt(promptMessage.c_str(), L"Retry", L"Cancel") == 0;
    } else {
        showDialogPrompt(promptMessage.c_str(), button);
        return false;
    }
}

void showSuccessPrompt(const wchar_t* message) {
    showDialogPrompt(message, L"OK");
}

static uint32_t read32LE(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool isRedNAND() {
    if (WHBMountSdCard() != 1) {
        return false;
    }
    std::string iniPath = Paths::SdMinuteDir + "/minute.ini";
    if (!fileExist(iniPath)) {
        WHBUnmountSdCard();
        return false;
    }

    int autoboot = -1;
    FILE* f = fileFopen(iniPath.c_str(), "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "autoboot=")) {
                sscanf(line, "autoboot=%d", &autoboot);
                break; // Found it
            }
        }
        fclose(f);
    }
    WHBUnmountSdCard();
    return autoboot == 2;
}

void uninstallStroopwafelMenu(bool showWarning) {
    if (showWarning) {
        if (isRedNAND()) {
            showDialogPrompt(L"redNAND is detected.\nStroopwafel is required for redNAND.\nUninstallation is not possible.", L"OK");
            return;
        }

        const wchar_t* warningMessage =
            L"Please read carefully:\n \n"
            L"This will remove Stroopwafel from your console.\n"
            L"Modifications made by other tools might still persist.\n \n"
            L"IMPORTANT: If you installed custom keyboards or themes, you\n"
            L"MUST undo these changes BEFORE uninstalling. Removing\n"
            L"Stroopwafel otherwise might cause a BRICK.\n";

        if (showDialogPrompt(warningMessage, L"Continue", L"Cancel", nullptr, nullptr, 1) != 0) {
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

void showStroopwafelMenu() {
    uint8_t selectedOption = 0;
    while(true) {
        CHECK_SHUTDOWN();

        WHBLogFreetypeStartScreen();
        WHBLogFreetypePrint(L"Stroopwafel Menu");
        WHBLogFreetypePrint(L"===============================");
        WHBLogFreetypePrintf(L"%C Download / Install Stroopwafel", OPTION(0));
        WHBLogFreetypePrintf(L"%C Uninstall Stroopwafel", OPTION(1));
        WHBLogFreetypePrint(L" ");
        WHBLogFreetypeScreenPrintBottom(L"===============================");
        WHBLogFreetypeScreenPrintBottom(L"\uE000 Button = Select Option \uE001 Button = Back");
        WHBLogFreetypeDrawScreen();

        updateInputs();
        while(true) {
            updateInputs();
            if (navigatedUp() && selectedOption > 0) {
                selectedOption--;
                break;
            }
            if (navigatedDown() && selectedOption < 1) {
                selectedOption++;
                break;
            }
            if (pressedOk()) {
                if (selectedOption == 0) {
                    installStroopwafelMenu();
                }
                else if (selectedOption == 1) {
                    uninstallStroopwafelMenu();
                }
                break;
            }
            if (pressedBack()) return;
        }
    }
}

void showUninstallMenu() {
    if (isRedNAND()) {
        showDialogPrompt(L"redNAND is detected.\nStroopwafel and ISFShax are required for redNAND.\nUninstallation is not possible.", L"OK");
        return;
    }

    const wchar_t* warningMessage =
        L"Please read carefully:\n \n"
        L"Reinstalling ISFShax won't fix any issue. It is recommended\n"
        L"to always keep ISFShax.\n \n"
        L"This will undo all modifications this tool might have made to\n"
        L"the console, turning it stock again. It will also offer to reset\n"
        L"the SD card if it was partitioned.\n"
        L"Modifications made by other tools might still persist.\n \n"
        L"IMPORTANT: If you installed custom keyboards or themes, you\n"
        L"MUST undo these changes BEFORE uninstalling. Removing\n"
        L"stroopwafel/isfshax otherwise might cause a BRICK.\n";

    if (showDialogPrompt(warningMessage, L"Continue", L"Cancel", nullptr, nullptr, 1) != 0) {
        return;
    }

    // SD Card Check
    if (WHBMountSdCard() == 1) {
        FSAClientHandle fsaHandle = FSAAddClient(nullptr);
        if (fsaHandle >= 0) {
            FSADeviceInfo deviceInfo;
            if ((FSStatus)FSAGetDeviceInfo(fsaHandle, "/dev/sdcard01", &deviceInfo) == FS_STATUS_OK) {
                uint8_t* mbr = (uint8_t*)memalign(0x40, deviceInfo.deviceSectorSize);
                if (mbr) {
                    bool askFormat = false;
                    IOSHandle handle = -1;
                    if (FSAEx_RawOpenEx(fsaHandle, "/dev/sdcard01", &handle) >= 0) {
                        if ((FSStatus)FSAEx_RawReadEx(fsaHandle, mbr, deviceInfo.deviceSectorSize, 1, 0, handle) == FS_STATUS_OK) {
                            if (mbr[510] == 0x55 && mbr[511] == 0xAA) {
                                int partitionCount = 0;
                                uint32_t lastOccupiedSector = 0;
                                for (int i = 0; i < 4; i++) {
                                    uint8_t type = mbr[446 + i * 16 + 4];
                                    if (type != 0) {
                                        partitionCount++;
                                        uint32_t start = read32LE(&mbr[446 + i * 16 + 8]);
                                        uint32_t sectors = read32LE(&mbr[446 + i * 16 + 12]);
                                        if (start + sectors > lastOccupiedSector) {
                                            lastOccupiedSector = start + sectors;
                                        }
                                    }
                                }
                                if (partitionCount > 1) {
                                    askFormat = true;
                                }
                                // If more than 1MB is unallocated
                                if (deviceInfo.deviceSizeInSectors > lastOccupiedSector + (1 * 1024 * 1024 / deviceInfo.deviceSectorSize)) {
                                    askFormat = true;
                                }
                            }
                        }
                        FSAEx_RawCloseEx(fsaHandle, handle);
                    }
                    free(mbr);

                    if (askFormat) {
                        if (showDialogPrompt(L"Your SD card seems to have multiple partitions or unallocated\nspace. Do you want to format the entire SD card to FAT32?\nThis will delete ALL data on it.", L"Yes, format SD", L"No, keep as is") == 0) {
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
                }
            }
            FSADelClient(fsaHandle);
        }
    }
    WHBUnmountSdCard();


    uninstallStroopwafelMenu(false);


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

