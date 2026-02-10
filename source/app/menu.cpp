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

// Menu screens

void showLoadingScreen() {
    WHBLogFreetypeSetBackgroundColor(0x0b5d5e00);
    WHBLogFreetypeSetFontColor(0xFFFFFFFF);
    WHBLogFreetypeSetFontSize(22);
    WHBLogPrint("ISFShax Loader");
    WHBLogPrint("-- Made by Crementif, Emiyl and zer00p Hofmeier --");
    WHBLogPrint("");
    WHBLogFreetypeDraw();
}

#define OPTION(n) (selectedOption == (n) ? L'>' : L' ')

static bool checkSystemAccess() {
    if (!isSlcMounted()) {
        showDialogPrompt(L"Cannot access the SLC!\nPlease make sure to disable 'System Access' in ftpiiu if you have it running.", L"OK");
        return false;
    }
    return true;
}

void installISFShax() {
    if (!checkSystemAccess()) return;

    bool isfshaxInstalled = isIsfshaxInstalled();
    bool stroopAvailable = isStroopwafelAvailable();

    if (!isfshaxInstalled) {
        if (downloadIsfshaxFiles()) {
            bool downloadStroop = true;
            if (stroopAvailable) {
                downloadStroop = (showDialogPrompt(L"Stroopwafel is already running.\nDo you want to download it again?", L"Yes", L"No") == 0);
            }

            if (downloadStroop) {
                bool toSD = (showDialogPrompt(L"Where do you want to download Stroopwafel?\nSD card is recommended.", L"SD Card", L"SLC") == 0);
                downloadStroopwafelFiles(toSD);
            }

            bootInstaller();
        }
    } else if (!stroopAvailable) {
        uint8_t choice = showDialogPrompt(L"Stroopwafel is missing or outdated.\nDo you want to download it?", L"Yes", L"No");
        if (choice == 0) {
            bool toSD = (showDialogPrompt(L"Where do you want to download Stroopwafel?\nSD card is recommended.", L"SD Card", L"SLC") == 0);
            downloadStroopwafelFiles(toSD);
        }
    } else {
        showDialogPrompt(L"ISFShax and Stroopwafel are already installed and running!", L"OK");
    }
}

void redownloadFiles() {
    while (true) {
        uint8_t choice = showDialogPrompt(L"What files do you want to redownload?", L"Everything", L"Stroopwafel only", L"ISFShax only", L"Cancel");
        if (choice == 3 || choice == 255) return;

        bool success = true;
        if (choice == 0 || choice == 1) {
            bool toSD = (showDialogPrompt(L"Where do you want to download Stroopwafel?\nSD card is recommended.", L"SD Card", L"SLC") == 0);
            success &= downloadStroopwafelFiles(toSD);
        }
        if (choice == 0 || choice == 2) {
            success &= downloadIsfshaxFiles();
        }

        if (success) {
            showSuccessPrompt(L"Files downloaded successfully!");
            break;
        } else {
            if (!showErrorPrompt(L"Cancel", true)) break;
        }
    }
}

void bootInstaller() {
    if (!checkSystemAccess()) return;

    std::string fwImgPath = convertToPosixPath("/vol/storage_slc/sys/hax/installer/fw.img");
    bool downloaded = true;
    if (!fileExist(fwImgPath.c_str())) {
        while (true) {
            uint8_t choice = showDialogPrompt(L"The ISFShax installer (fw.img) is missing.\nDo you want to download everything or just the installer?", L"Everything", L"Just installer", L"Cancel");
            if (choice == 2) return;

            if (choice == 0) {
                downloaded = downloadHaxFiles();
            } else {
                downloaded = downloadInstallerOnly();
            }

            if (!downloaded) {
                if (!showErrorPrompt(L"Cancel", true)) return;
            } else {
                break;
            }
        }
    }

    if (downloaded) {
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

// Can get recursively called
void showMainMenu() {
    uint8_t selectedOption = 0;
    bool startSelectedOption = false;
    while(!startSelectedOption) {
        // Print menu text
        WHBLogFreetypeStartScreen();
        WHBLogFreetypePrint(L"ISFShax Loader");
        WHBLogFreetypePrint(L"===============================");
        WHBLogFreetypePrintf(L"%C Install ISFShax + sd emulation + payloadler", OPTION(0));
        WHBLogFreetypePrintf(L"%C Redownload files", OPTION(1));
        WHBLogFreetypePrintf(L"%C Boot Installer", OPTION(2));
        WHBLogFreetypePrintf(L"%C Download Aroma", OPTION(3));
        WHBLogFreetypePrintf(L"%C Format and Partition", OPTION(4));
        WHBLogFreetypePrintf(L"%C Set up SDUSB", OPTION(5));
        WHBLogFreetypePrint(L"");
        WHBLogFreetypePrintf(L"%C Stroopwafel Plugin Manager", OPTION(6));
        WHBLogFreetypeScreenPrintBottom(L"===============================");
        WHBLogFreetypeScreenPrintBottom(L"\uE000 Button = Select Option \uE001 Button = Exit ISFShax Loader");
        WHBLogFreetypeScreenPrintBottom(L"");
        WHBLogFreetypeDrawScreen();

        // Loop until there's new input
        sleep_for(200ms); // Cooldown between each button press
        updateInputs();
        while(!startSelectedOption) {
            updateInputs();
            // Check each button state
            if (navigatedUp()) {
                if (selectedOption == 6) {
                    selectedOption = 5;
                    break;
                } else if (selectedOption > 0) {
                    selectedOption--;
                    break;
                }
            }
            if (navigatedDown()) {
                if (selectedOption == 5) {
                    selectedOption = 6;
                    break;
                } else if (selectedOption < 5) {
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
            installISFShax();
            break;
        case 1:
            redownloadFiles();
            break;
        case 2:
            bootInstaller();
            break;
        case 3:
            installAromaMenu();
            break;
        case 4:
            formatAndPartitionMenu();
            break;
        case 5:
            setupSDUSBMenu();
            break;
        case 6:
            showPluginManager();
            break;
        default:
            break;
    }

    sleep_for(500ms);
    showMainMenu();
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

        WHBLogFreetypePrint(L"");
        for (uint8_t i = 0; i < numButtons; i++) {
            WHBLogFreetypePrintf(L"%C [%S]", OPTION(i), buttons[i].c_str());
        }
        WHBLogFreetypePrint(L"");
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
