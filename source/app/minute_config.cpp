#include "minute_config.h"
#include "menu.h"
#include "navigation.h"
#include "gui.h"
#include "filesystem.h"
#include "common.h"
#include <whb/sdcard.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <unistd.h>

namespace fs = std::filesystem;

#define OPTION(n) (selectedOption == (n) ? L'>' : L' ')

void ensureMinuteIni() {
    WHBMountSdCard();
    std::string minuteDir = "fs:/vol/external01/minute";
    std::string iniPath = minuteDir + "/minute.ini";
    fs::create_directories(minuteDir);
    if (!fileExist(iniPath.c_str())) {
        int fd = open(iniPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            const char* content = "[boot]\nautoboot=3\nautoboot_timeout=0\n";
            write(fd, content, strlen(content));
            close(fd);
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
