#include "menu.h"
#include "navigation.h"
#include "filesystem.h"
#include "gui.h"
#include "cfw.h"
#include "fw_img_loader.h"
#include "download.h"

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

void installISFShax() {
    if (downloadHaxFiles()) {
        showDialogPrompt(L"The ISFShax installer is controlled with the buttons on the main console.\nPOWER: moves the curser\nEJECT: confirm\nPress A to launch into the ISFShax Installer", L"Continue");
        loadFwImg();
    } else {
        showDialogPrompt(L"Failed to download all files.\nPlease check your internet connection and try again.", L"OK");
    }
}

void redownloadFiles() {
    if (downloadHaxFiles()) {
        showDialogPrompt(L"All hax files downloaded successfully!", L"OK");
    } else {
        showDialogPrompt(L"Failed to download all files.\nPlease check your internet connection and try again.", L"OK");
    }
}

void bootInstaller() {
    std::string fwImgPath = convertToPosixPath("/vol/storage_slc/sys/hax/installer/fw.img");
    bool downloaded = true;
    if (!fileExist(fwImgPath.c_str())) {
        uint8_t choice = showDialogPrompt(L"The ISFShax installer (fw.img) is missing.\nDo you want to download everything or just the installer?", L"Everything", L"Just installer");
        if (choice == 0) {
            downloaded = downloadHaxFiles();
        } else {
            downloaded = downloadInstallerOnly();
        }

        if (!downloaded) {
             showDialogPrompt(L"Failed to download the required files.\nPlease check your internet connection and try again.", L"OK");
             return;
        }
    }

    if (downloaded) {
        showDialogPrompt(L"The ISFShax installer is controlled with the buttons on the main console.\nPOWER: moves the curser\nEJECT: confirm\nPress A to launch into the ISFShax Installer", L"Continue");
        loadFwImg();
    }
}

void installAromaMenu() {
    if (downloadAroma()) {
        showDialogPrompt(L"Aroma downloaded and extracted successfully!", L"OK");
    } else {
        showDialogPrompt(L"Failed to download or extract Aroma.\nPlease check your internet connection and try again.", L"OK");
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
            if (navigatedUp() && selectedOption > 0) {
                selectedOption--;
                break;
            }
            if (navigatedDown() && selectedOption < 3) {
                selectedOption++;
                break;
            }
            if (pressedOk()) {
                startSelectedOption = true;
                break;
            }
            if (pressedBack()) {
                uint8_t exitSelectedOption = showDialogPrompt(getCFWVersion() == MOCHA_FSCLIENT ? L"Do you really want to exit ISFShax Loader?" : L"Do you really want to exit ISFShax Loader?\nYour console will reboot to prevent compatibility issues!", L"Yes", L"No");
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
        default:
            break;
    }

    sleep_for(500ms);
    showMainMenu();
}


// Helper functions

uint8_t showDialogPrompt(const wchar_t* message, const wchar_t* button1, const wchar_t* button2) {
    sleep_for(100ms);
    uint8_t selectedOption = 0;
    while(true) {
        WHBLogFreetypeStartScreen();

        // Print each line
        std::wistringstream messageStream(message);
        std::wstring line;

        while(std::getline(messageStream, line)) {
            WHBLogFreetypePrint(line.c_str());
        }

        WHBLogFreetypePrint(L"");
        WHBLogFreetypePrintf(L"%C [%S]", OPTION(0), button1);
        if (button2 != nullptr) WHBLogFreetypePrintf(L"%C [%S]", OPTION(1), button2);
        WHBLogFreetypePrint(L"");
        WHBLogFreetypeScreenPrintBottom(L"===============================");
        WHBLogFreetypeScreenPrintBottom(L"\uE000 Button = Select Option");
        WHBLogFreetypeDrawScreen();

        // Input loop
        sleep_for(400ms);
        updateInputs();
        while (true) {
            updateInputs();
            // Handle navigation between the buttons
            if (button2 != nullptr) {
                if (navigatedUp() && selectedOption == 1) {
                    selectedOption = 0;
                    break;
                }
                else if (navigatedDown() && selectedOption == 0) {
                    selectedOption = 1;
                    break;
                }
            }

            if (pressedOk()) {
                return selectedOption;
            }

            sleep_for(50ms);
        }
    }
}

void showDialogPrompt(const wchar_t* message, const wchar_t* button) {
    showDialogPrompt(message, button, nullptr);
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

void showErrorPrompt(const wchar_t* button) {
    std::wstring promptMessage(L"An error occurred:\n");
    if (errorMessage) promptMessage += errorMessage;
    else promptMessage += L"No error was specified!";
    showDialogPrompt(promptMessage.c_str(), button);
}
