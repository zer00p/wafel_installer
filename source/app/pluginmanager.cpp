#include "pluginmanager.h"
#include "menu.h"
#include "gui.h"
#include "navigation.h"
#include "filesystem.h"
#include "cfw.h"
#include "download.h"
#include <dirent.h>
#include <algorithm>
#include <vector>
#include <string>
#include <sstream>
#include <unistd.h>
#include <stroopwafel/stroopwafel.h>

#define OPTION(n) (selectedOption == (n) ? L'>' : L' ')

struct Plugin {
    std::string shortDescription;
    std::string fileName;
    std::string downloadPath;
    std::string longDescription;
    std::string incompatiblePlugins;
};

static std::vector<Plugin> cachedPluginList;
static bool failedToFetch = false;

static bool fetchPluginList(bool force = false) {
    if (!cachedPluginList.empty()) return true;
    if (failedToFetch && !force) return false;

    std::string csvData;
    std::string url = "https://raw.githubusercontent.com/zer00p/isfshax-loader/refs/heads/master/plugins.csv";

    if (!downloadToBuffer(url, csvData)) {
        // downloadToBuffer already handles retries/cancel internally via showErrorPrompt
        failedToFetch = true;
        return false;
    }
    failedToFetch = false;

    std::stringstream ss(csvData);
    std::string line;
    // skip header
    if (!std::getline(ss, line)) return false;

    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        if (line.back() == '\r') line.pop_back(); // Handle Windows line endings

        std::stringstream lineStream(line);
        std::string cell;
        std::vector<std::string> cells;
        while (std::getline(lineStream, cell, ';')) {
            cells.push_back(cell);
        }
        if (cells.size() >= 4) {
            Plugin p;
            p.shortDescription = cells[0];
            p.fileName = cells[1];
            p.downloadPath = cells[2];
            p.longDescription = cells[3];
            if (cells.size() >= 5) p.incompatiblePlugins = cells[4];
            cachedPluginList.push_back(p);
        }
    }

    return !cachedPluginList.empty();
}

static bool browsePlugins(std::string posixPath) {
    if (!fetchPluginList(true)) return false;

    uint8_t selectedOption = 0;
    while (true) {
        WHBLogFreetypeStartScreen();
        WHBLogFreetypePrint(L"Available Plugins");
        WHBLogFreetypePrint(L"===============================");

        for (size_t i = 0; i < cachedPluginList.size(); i++) {
            const auto& p = cachedPluginList[i];
            bool installed = false;
            std::string fullPath = posixPath;
            if (fullPath.back() != '/') fullPath += "/";
            fullPath += p.fileName;
            if (access(fullPath.c_str(), F_OK) == 0) {
                installed = true;
            }

            WHBLogFreetypePrintf(L"%C %S %S", OPTION(i), toWstring(p.fileName).c_str(), installed ? L"(Installed)" : L"");
            WHBLogFreetypePrintf(L"  %S", toWstring(p.shortDescription).c_str());
        }

        WHBLogFreetypePrint(L"");
        WHBLogFreetypePrint(L"Description:");
        // Manually wrap long description
        std::string desc = cachedPluginList[selectedOption].longDescription;
        while (desc.length() > 0) {
            std::string line = desc.substr(0, 64);
            if (desc.length() > 64) {
                size_t lastSpace = line.find_last_of(' ');
                if (lastSpace != std::string::npos) {
                    line = desc.substr(0, lastSpace);
                    desc = desc.substr(lastSpace + 1);
                } else {
                    desc = desc.substr(64);
                }
            } else {
                desc = "";
            }
            WHBLogFreetypePrintf(L" %S", toWstring(line).c_str());
        }

        WHBLogFreetypeScreenPrintBottom(L"===============================");
        WHBLogFreetypeScreenPrintBottom(L"\uE000 Download \uE001 Back");

        WHBLogFreetypeDrawScreen();

        sleep_for(100ms);
        updateInputs();
        while (true) {
            updateInputs();
            if (navigatedUp() && selectedOption > 0) {
                selectedOption--;
                break;
            }
            if (navigatedDown() && selectedOption < cachedPluginList.size() - 1) {
                selectedOption++;
                break;
            }
            if (pressedBack()) {
                return false;
            }
            if (pressedOk()) {
                const auto& p = cachedPluginList[selectedOption];
                std::wstring msg = L"Do you want to download " + toWstring(p.fileName) + L"?";
                if (showDialogPrompt(msg.c_str(), L"Yes", L"No") == 0) {
                    std::string fullPath = posixPath;
                    if (fullPath.back() != '/') fullPath += "/";
                    fullPath += p.fileName;
                    if (downloadFile(p.downloadPath, fullPath)) {
                        showSuccessPrompt(L"Plugin downloaded successfully!");
                        return true;
                    }
                }
                break;
            }
            sleep_for(50ms);
        }
    }
}

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

        fetchPluginList();
        if (plugins.empty()) {
            WHBLogFreetypePrint(L"No plugins found.");
        } else {
            for (size_t i = 0; i < plugins.size(); i++) {
                std::string shortDesc = "";
                for (const auto& p : cachedPluginList) {
                    if (p.fileName == plugins[i]) {
                        shortDesc = " - " + p.shortDescription;
                        break;
                    }
                }
                WHBLogFreetypePrintf(L"%C %S%S", OPTION(i), toWstring(plugins[i]).c_str(), toWstring(shortDesc).c_str());
            }
        }

        WHBLogFreetypeScreenPrintBottom(L"===============================");
        WHBLogFreetypeScreenPrintBottom(L"\uE000 Select \uE001 Back \uE002 Delete \uE003 Get plugin");
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
            if (pressedY()) {
                if (browsePlugins(posixPath)) {
                    refreshList = true;
                }
                break;
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
