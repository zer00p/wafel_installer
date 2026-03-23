#include "minute_config.h"
#include "menu.h"
#include "navigation.h"
#include "gui.h"
#include "filesystem.h"
#include "common_paths.h"
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
    std::string minuteDir = Paths::SdMinuteDir;
    std::string iniPath = minuteDir + "/minute.ini";
    createDirectories(minuteDir);
    if (!fileExist(iniPath)) {
        int fd = fileOpen(iniPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            const char* content = "[boot]\nautoboot=3\nautoboot_timeout=0\nodd_power=true\n";
            write(fd, content, strlen(content));
            close(fd);
        }
    }
}

void configureMinuteMenu() {
    WHBMountSdCard();
    std::string iniPath = Paths::SdMinuteDir + "/minute.ini";

    int autoboot = 3;
    int timeout = 0;
    bool odd_power = true;

    // Read current values
    FILE* f = fileFopen(iniPath.c_str(), "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "autoboot_timeout=")) {
                sscanf(line, "autoboot_timeout=%d", &timeout);
            } else if (strstr(line, "autoboot=")) {
                sscanf(line, "autoboot=%d", &autoboot);
            } else if (strstr(line, "odd_power=")) {
                char val[16];
                if (sscanf(line, "odd_power=%15s", val) == 1) {
                    if (strcmp(val, "false") == 0) odd_power = false;
                    else if (strcmp(val, "true") == 0) odd_power = true;
                }
            }
        }
        fclose(f);
    }

    uint8_t selectedOption = 0;
    while (true) {
        WHBLogFreetypeStartScreen();
        WHBLogFreetypePrint(L"Configure Minute");
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
        WHBLogFreetypePrintf(L"%C ODD Power: %S", OPTION(2), odd_power ? L"Enabled" : L"Disabled");
        WHBLogFreetypePrint(L" ");
        WHBLogFreetypePrintf(L"%C Save and Exit", OPTION(3));

        WHBLogFreetypeScreenPrintBottom(L"===============================");
        WHBLogFreetypeScreenPrintBottom(L"\u2191/\u2193 = Move, \u2190/\u2192 = Change Timeout/ODD, \uE000 = Select, \uE001 = Back");
        WHBLogFreetypeDrawScreen();

        sleep_for(100ms);
        updateInputs();
        while (true) {
            updateInputs();
            if (navigatedUp() && selectedOption > 0) {
                selectedOption--;
                break;
            }
            if (navigatedDown() && selectedOption < 3) {
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
                    odd_power = !odd_power;
                    break;
                } else if (selectedOption == 3) {
                    // Save
                    std::string minuteDir = Paths::SdMinuteDir;
                    createDirectories(minuteDir);
                    FILE* f = fileFopen(iniPath.c_str(), "w");
                    if (f) {
                        fprintf(f, "[boot]\nautoboot=%d\nautoboot_timeout=%d\nodd_power=%s\n", autoboot, timeout, odd_power ? "true" : "false");
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
            if (selectedOption == 2) {
                if (navigatedRight() || navigatedLeft()) {
                    odd_power = !odd_power;
                    break;
                }
            }

            sleep_for(50ms);
        }
    }
}
