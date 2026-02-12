#include "menu.h"
#include "pluginmanager.h"
#include "partition_manager.h"
#include "navigation.h"
#include "filesystem.h"
#include "gui.h"
#include "cfw.h"
#include "fw_img_loader.h"
#include "download.h"
#include <vector>
#include <string>
#include <dirent.h>
#include <whb/sdcard.h>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

// Menu screens

void showLoadingScreen() {
    WHBLogFreetypeSetBackgroundColor(0x0b5d5e00);
    WHBLogFreetypeSetFontColor(0xFFFFFFFF);
    WHBLogFreetypeSetFontSize(22);
    WHBLogPrint("ISFShax Loader");
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

    const char* cfwDir = "fs:/vol/external01/wiiu/cfw";
    if (!dirExist(cfwDir)) {
        if (mkdir(cfwDir, 0755) != 0 && errno != EEXIST) {
            setErrorPrompt(L"Failed to create wiiu/cfw directory on SD!");
            showErrorPrompt(L"OK");
            return;
        }
        showDialogPrompt(L"The directory 'wiiu/cfw' has been created on your SD card.\nPlease put your fw.img files there to boot them.", L"OK");
        return;
    }

    DIR* dir = opendir(cfwDir);
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
        std::string fullPath = "/vol/sdcard/wiiu/cfw/" + toString(files[choice]);
        loadFwImg(fullPath.c_str());
    }
}

void installStroopwafelMenu() {
    bool sdEmulated = isSdEmulated();
    bool toSD = false;

    if (sdEmulated) {
        uint8_t choice = showDialogPrompt(L"Where do you want to download Stroopwafel?\nNote: Stroopwafel cannot be installed to the USB device, even when using SD emulation.", L"SLC", L"Cancel");
        if (choice != 0) return;
        toSD = false;
    } else {
        uint8_t choice = showDialogPrompt(L"Where do you want to download Stroopwafel?\nSD card is recommended.", L"SD Card", L"SLC", L"Cancel");
        if (choice == 2 || choice == 255) return;
        toSD = (choice == 0);
    }

    if (!toSD && !checkSystemAccess()) return;

    if (downloadStroopwafelFiles(toSD)) {
        showSuccessPrompt(L"Stroopwafel files downloaded successfully!");

        if (!isIsfshaxInstalled()) {
            if (showDialogPrompt(L"ISFShax is not detected.\nDo you want to download the ISFShax installer and superblock files now?", L"Yes", L"No") == 0) {
                if (downloadIsfshaxFiles()) {
                    bootInstaller();
                }
            }
        }
    } else {
        showErrorPrompt(L"OK");
    }
}

void installIsfshaxMenu() {
    if (!checkSystemAccess()) return;

    uint8_t downloadChoice = showDialogPrompt(L"Do you want to download the latest ISFShax files (installer and superblock)?", L"Yes", L"No", L"Cancel");
    if (downloadChoice == 2 || downloadChoice == 255) return;

    bool downloaded = true;
    if (downloadChoice == 0) {
        downloaded = downloadIsfshaxFiles();
    } else {
        std::string fwImgPath = convertToPosixPath("/vol/storage_slc/sys/hax/installer/fw.img");
        if (!fileExist(fwImgPath.c_str())) {
            uint8_t missingChoice = showDialogPrompt(L"The ISFShax installer (fw.img) is missing.", L"Download", L"Cancel");
            if (missingChoice == 0) {
                downloaded = downloadIsfshaxFiles();
            } else {
                return;
            }
        }
    }

    if (downloaded) {
        bootInstaller();
    }
}

void bootInstaller() {
    if (!checkSystemAccess()) return;

    std::string fwImgPath = convertToPosixPath("/vol/storage_slc/sys/hax/installer/fw.img");
    if (!fileExist(fwImgPath.c_str())) {
        setErrorPrompt(L"ISFShax installer (fw.img) is missing!");
        showErrorPrompt(L"OK");
        return;
    }

    while (true) {
        sleep_for(1s);
        uint8_t choice = showDialogPrompt(L"The ISFShax installer is controlled with the buttons on the main console.\nPOWER: moves the curser\nEJECT: confirm\nPress A to launch into the ISFShax Installer", L"Continue", L"Cancel");
        if (choice == 0) {
            loadFwImg();
            break;
        } else {
            if (showDialogPrompt(L"Are you sure? ISFShax is required for stroopwafel", L"Yes, cancel", L"No, go back") == 0) {
                return;
            }
        }
    }
}

void installAromaMenu() {
    while (true) {
        if (downloadAroma()) {
            showSuccessPrompt(L"Aroma and tools downloaded and extracted successfully!");
            break;
        } else {
            if (!showErrorPrompt(L"Cancel", true)) break;
        }
    }
}

void configureMinuteMenu() {
    WHBMountSdCard();
    std::string iniPath = "fs:/vol/external01/minute/minute.ini";

    int autoboot = 3;
    int timeout = 0;

    // Read current values
    FILE* f = fopen(iniPath.c_str(), "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "autoboot_timeout=")) {
                sscanf(line, "autoboot_timeout=%d", &timeout);
            } else if (strstr(line, "autoboot=")) {
                sscanf(line, "autoboot=%d", &autoboot);
            }
        }
        fclose(f);
    }

    uint8_t selectedOption = 0;
    while (true) {
        WHBLogFreetypeStartScreen();
        WHBLogFreetypePrint(L"Configure Minute Autoboot");
        WHBLogFreetypePrint(L"===============================");

        std::wstring autobootStr;
        switch(autoboot) {
            case 0: autobootStr = L"Disabled"; break;
            case 1: autobootStr = L"Plugins from SLC"; break;
            case 2: autobootStr = L"redNAND"; break;
            case 3: autobootStr = L"Plugins from SD"; break;
            default: autobootStr = L"Unknown (" + std::to_wstring(autoboot) + L")"; break;
        }

        WHBLogFreetypePrintf(L"%C Autoboot: %S", OPTION(0), autobootStr.c_str());
        WHBLogFreetypePrintf(L"%C Timeout: %d seconds", OPTION(1), timeout);
        WHBLogFreetypePrint(L" ");
        WHBLogFreetypePrintf(L"%C Save and Exit", OPTION(2));

        WHBLogFreetypeScreenPrintBottom(L"===============================");
        WHBLogFreetypeScreenPrintBottom(L"\u2191/\u2193 = Move, \u2190/\u2192 = Change Timeout, \uE000 = Select, \uE001 = Back");
        WHBLogFreetypeDrawScreen();

        sleep_for(100ms);
        updateInputs();
        while (true) {
            updateInputs();
            if (navigatedUp() && selectedOption > 0) {
                selectedOption--;
                break;
            }
            if (navigatedDown() && selectedOption < 2) {
                selectedOption++;
                break;
            }
            if (pressedOk()) {
                if (selectedOption == 0) {
                    std::vector<std::wstring> options = {L"Disabled (0)", L"Plugins from SLC (1)", L"redNAND (2)", L"Plugins from SD (3)"};
                    uint8_t choice = showDialogPrompt(L"Select Autoboot Option:", options, (autoboot >= 0 && autoboot <= 3) ? autoboot : 3);
                    if (choice != 255) {
                        autoboot = choice;
                    }
                    break;
                } else if (selectedOption == 1) {
                    timeout++;
                    break;
                } else if (selectedOption == 2) {
                    // Save
                    std::string minuteDir = "fs:/vol/external01/minute";
                    fs::create_directories(minuteDir);
                    FILE* f = fopen(iniPath.c_str(), "w");
                    if (f) {
                        fprintf(f, "[boot]\nautoboot=%d\nautoboot_timeout=%d\n", autoboot, timeout);
                        fclose(f);
                        showSuccessPrompt(L"Configuration saved!");
                        return;
                    } else {
                        setErrorPrompt(L"Failed to open minute.ini for writing!");
                        showErrorPrompt(L"OK");
                        break;
                    }
                }
            }
            if (pressedBack()) {
                return;
            }
            if (selectedOption == 1) {
                if (navigatedRight()) {
                    timeout++;
                    break;
                }
                if (navigatedLeft() && timeout > 0) {
                    timeout--;
                    break;
                }
            }

            sleep_for(50ms);
        }
    }
}

void showMainMenu() {
    uint8_t selectedOption = 0;
    while(true) {
        if (isShutdownForced()) return;

        bool startSelectedOption = false;
        while(!startSelectedOption) {
            // Print menu text
            WHBLogFreetypeStartScreen();
            WHBLogFreetypePrint(L"ISFShax Loader");
            WHBLogFreetypePrint(L"===============================");
            WHBLogFreetypePrintf(L"%C Stroopwafel Plugin Manager", OPTION(0));
            WHBLogFreetypePrintf(L"%C Load custom fw.img", OPTION(1));
            WHBLogFreetypePrintf(L"%C Install Stroopwafel", OPTION(2));
            WHBLogFreetypePrintf(L"%C (Un)Install ISFShax", OPTION(3));
            WHBLogFreetypePrintf(L"%C Check for Updates", OPTION(4));
            WHBLogFreetypePrintf(L"%C Download Aroma", OPTION(5));
            WHBLogFreetypePrintf(L"%C Format and Partition", OPTION(6));
            WHBLogFreetypePrintf(L"%C Set up SDUSB", OPTION(7));
            WHBLogFreetypePrintf(L"%C Set up USB Partition", OPTION(8));
            WHBLogFreetypePrintf(L"%C Configure Minute Autoboot", OPTION(9));
            WHBLogFreetypePrint(L" ");

            WHBLogFreetypeScreenPrintBottom(L"===============================");
            if (!isStroopwafelAvailable()) {
                WHBLogFreetypeScreenPrintBottom(L"Stroopwafel: Not active");
            } else {
                std::wstring path = toWstring(getStroopwafelPluginPosixPath());
                if (path.empty()) {
                    WHBLogFreetypeScreenPrintBottom(L"Stroopwafel: Active (Path unknown)");
                } else {
                    WHBLogFreetypeScreenPrintBottom((L"Plugins: " + path).c_str());
                }
            }
            WHBLogFreetypeScreenPrintBottom(L"\uE000 Button = Select Option \uE001 Button = Exit ISFShax Loader");
            WHBLogFreetypeScreenPrintBottom(L"");
            WHBLogFreetypeDrawScreen();

            // Loop until there's new input
            sleep_for(200ms); // Cooldown between each button press
            updateInputs();
            while(!startSelectedOption) {
                if (isShutdownForced()) return;
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
                    uint8_t exitSelectedOption = showDialogPrompt(getCFWVersion() == MOCHA_FSCLIENT ? L"Do you really want to exit ISFShax Loader?" : L"Do you really want to exit ISFShax Loader?\nYour console will reboot to prevent compatibility issues!", L"Yes", L"No", nullptr, nullptr, 1);
                    if (exitSelectedOption == 0) {
                        WHBLogFreetypeClear();
                        return;
                    }
                    else break;
                }
                sleep_for(50ms);
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
                installStroopwafelMenu();
                break;
            case 3:
                installIsfshaxMenu();
                break;
            case 4:
                checkForUpdates();
                break;
            case 5:
                installAromaMenu();
                break;
            case 6:
                formatAndPartitionMenu();
                break;
            case 7:
                setupSDUSBMenu();
                break;
            case 8:
                setupPartitionedUSBMenu();
                break;
            case 9:
                configureMinuteMenu();
                break;
            default:
                break;
        }

        sleep_for(500ms);
    }
}


// Helper functions

uint8_t showDialogPrompt(const wchar_t* message, const std::vector<std::wstring>& buttons, uint8_t defaultOption, bool clearScreen) {
    sleep_for(100ms);
    uint8_t selectedOption = defaultOption;
    uint8_t numButtons = buttons.size();
    uint32_t startPos = clearScreen ? 0 : WHBLogFreetypeGetScreenPosition();

    while(true) {
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
        sleep_for(400ms);
        updateInputs();
        while (true) {
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

            sleep_for(50ms);
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
        sleep_for(50ms);
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
    sleep_for(2s);
    showDialogPrompt(message, L"OK");
}
