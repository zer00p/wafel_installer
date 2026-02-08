#include "pluginmanager.h"
#include "menu.h"
#include "gui.h"
#include "navigation.h"
#include "filesystem.h"
#include "cfw.h"
#include <dirent.h>
#include <algorithm>
#include <vector>
#include <string>
#include <stroopwafel/stroopwafel.h>

#define OPTION(n) (selectedOption == (n) ? L'>' : L' ')

static void managePlugins(std::string posixPath) {
    uint8_t selectedOption = 0;
    bool refreshList = true;
    std::vector<std::string> plugins;

    while(true) {
        if (refreshList) {
            plugins.clear();
            DIR* dir = opendir(posixPath.c_str());
            if (dir) {
                struct dirent* ent;
                while ((ent = readdir(dir)) != nullptr) {
                    if (ent->d_type == DT_REG) {
                        plugins.push_back(ent->d_name);
                    }
                }
                closedir(dir);
            }
            std::sort(plugins.begin(), plugins.end());
            refreshList = false;
        }

        WHBLogFreetypeStartScreen();
        WHBLogFreetypePrintf(L"Managing: %S", toWstring(posixPath).c_str());
        WHBLogFreetypePrint(L"===============================");

        if (plugins.empty()) {
            WHBLogFreetypePrint(L"No plugins found.");
        } else {
            for (size_t i = 0; i < plugins.size(); i++) {
                WHBLogFreetypePrintf(L"%C %S", OPTION(i), toWstring(plugins[i]).c_str());
            }
        }

        WHBLogFreetypeScreenPrintBottom(L"===============================");
        WHBLogFreetypeScreenPrintBottom(L"\uE000 Select \uE001 Back \uE002 Delete Plugin");
        WHBLogFreetypeDrawScreen();

        sleep_for(100ms);
        updateInputs();
        while(true) {
            updateInputs();
            if (navigatedUp() && selectedOption > 0) {
                selectedOption--;
                break;
            }
            if (navigatedDown() && !plugins.empty() && selectedOption < plugins.size() - 1) {
                selectedOption++;
                break;
            }
            if (pressedBack()) {
                return;
            }
            if (pressedX() && !plugins.empty()) {
                std::string pluginName = plugins[selectedOption];
                if (pluginName == "00core.ipx" || pluginName == "5isfshax.ipx") {
                    showDialogPrompt(L"This plugin is protected and cannot be deleted!", L"OK");
                } else {
                    std::wstring msg = L"Do you really want to delete " + toWstring(pluginName) + L"?";
                    if (showDialogPrompt(msg.c_str(), L"Yes", L"No") == 0) {
                        std::string fullPath = posixPath;
                        if (fullPath.back() != '/') fullPath += "/";
                        fullPath += pluginName;
                        if (remove(fullPath.c_str()) == 0) {
                            showSuccessPrompt(L"Plugin deleted successfully!");
                            if (selectedOption > 0 && selectedOption == plugins.size() - 1) {
                                selectedOption--;
                            }
                            refreshList = true;
                        } else {
                            setErrorPrompt(L"Failed to delete plugin!");
                            showErrorPrompt(L"OK");
                        }
                    }
                }
                break;
            }
            sleep_for(50ms);
        }
    }
}

void showPluginManager() {
    StroopwafelMinutePath currentPath = {0};
    bool stroopAvailable = isStroopwafelAvailable();
    if (stroopAvailable) {
        Stroopwafel_GetPluginPath(&currentPath);
    }

    std::string slcDefaultPath = "/sys/hax/ios_plugins";
    std::string sdDefaultPath = "/wiiu/ios_plugins";

    std::string slcPosix = convertToPosixPath("/vol/storage_slc/sys/hax/ios_plugins");
    std::string sdPosix = convertToPosixPath("/vol/external01/wiiu/ios_plugins");

    std::string currentPosix;
    if (stroopAvailable) {
        if (currentPath.device == STROOPWAFEL_MIN_DEV_SLC) {
            currentPosix = convertToPosixPath(("/vol/storage_slc" + std::string(currentPath.path)).c_str());
        } else if (currentPath.device == STROOPWAFEL_MIN_DEV_SD) {
            currentPosix = convertToPosixPath(("/vol/external01" + std::string(currentPath.path)).c_str());
        }
    }

    std::vector<std::pair<std::wstring, std::string>> options;
    options.push_back({L"SLC Plugins (/sys/hax/ios_plugins)", slcPosix});
    options.push_back({L"SD Plugins (/wiiu/ios_plugins)", sdPosix});

    uint8_t selectedOption = 0;
    if (stroopAvailable && !currentPosix.empty()) {
        bool isSLCDefault = (currentPath.device == STROOPWAFEL_MIN_DEV_SLC && std::string(currentPath.path) == slcDefaultPath);
        bool isSDDefault = (currentPath.device == STROOPWAFEL_MIN_DEV_SD && std::string(currentPath.path) == sdDefaultPath);

        if (isSLCDefault) {
            selectedOption = 0;
        } else if (isSDDefault) {
            selectedOption = 1;
        } else {
            std::wstring devName = (currentPath.device == STROOPWAFEL_MIN_DEV_SLC ? L"SLC" : L"SD");
            options.push_back({L"Current Plugins (" + devName + L": " + toWstring(currentPath.path) + L")", currentPosix});
            selectedOption = 2;
        }
    }

    while(true) {
        WHBLogFreetypeStartScreen();
        WHBLogFreetypePrint(L"Stroopwafel Plugin Manager");
        WHBLogFreetypePrint(L"===============================");
        for (size_t i = 0; i < options.size(); i++) {
            WHBLogFreetypePrintf(L"%C %S", OPTION(i), options[i].first.c_str());
        }
        WHBLogFreetypeScreenPrintBottom(L"===============================");
        WHBLogFreetypeScreenPrintBottom(L"\uE000 Button = Select Option \uE001 Button = Back");
        WHBLogFreetypeDrawScreen();

        sleep_for(100ms);
        updateInputs();
        while(true) {
            updateInputs();
            if (navigatedUp() && selectedOption > 0) {
                selectedOption--;
                break;
            }
            if (navigatedDown() && selectedOption < options.size() - 1) {
                selectedOption++;
                break;
            }
            if (pressedOk()) {
                managePlugins(options[selectedOption].second);
                break;
            }
            if (pressedBack()) {
                return;
            }
            sleep_for(50ms);
        }
    }
}
