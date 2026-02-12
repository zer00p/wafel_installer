#include "pluginmanager.h"
#include "menu.h"
#include "gui.h"
#include "navigation.h"
#include "filesystem.h"
#include "cfw.h"
#include "download.h"
#include "../utils/sha256.h"
#include <dirent.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>
#include <sstream>
#include <unistd.h>
#include <filesystem>
#include <stroopwafel/stroopwafel.h>

#define OPTION(n) (selectedOption == (n) ? L'>' : L' ')

static bool browsePlugins(std::string posixPath) {
    if (!fetchPluginList(true)) return false;

    bool changed = false;
    const auto& cachedPluginList = getCachedPluginList();
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

            std::wstring fileName = toWstring(p.fileName);
            if (fileName.length() < 16) fileName.append(16 - fileName.length(), L' ');
            fileName += L" ";

            WHBLogFreetypePrintf(L"%C %S %S %S", OPTION(i), fileName.c_str(), toWstring(p.shortDescription).c_str(), installed ? L"(Installed)" : L"");
        }

        WHBLogFreetypePrint(L" ");
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
                return changed;
            }
            if (pressedOk()) {
                const auto& p = cachedPluginList[selectedOption];

                // Check for incompatible plugins
                if (!p.incompatiblePlugins.empty()) {
                    std::stringstream ss(p.incompatiblePlugins);
                    std::string incompatibleFile;
                    while (std::getline(ss, incompatibleFile, ',')) {
                        // trim whitespace
                        size_t first = incompatibleFile.find_first_not_of(" ");
                        if (std::string::npos == first) continue;
                        size_t last = incompatibleFile.find_last_not_of(" ");
                        incompatibleFile = incompatibleFile.substr(first, (last - first + 1));

                        std::string fullPath = posixPath;
                        if (fullPath.back() != '/') fullPath += "/";
                        fullPath += incompatibleFile;
                        if (access(fullPath.c_str(), F_OK) == 0) {
                            std::wstring msg = L"Warning: " + toWstring(incompatibleFile) + L" is already installed and is incompatible with " + toWstring(p.fileName) + L"!\nDo you want to delete the incompatible plugin first?";
                            uint8_t res = showDialogPrompt(msg.c_str(), L"Delete", L"Keep both", L"Cancel");
                            if (res == 0) { // Delete
                                if (remove(fullPath.c_str()) == 0) {
                                    showSuccessPrompt(L"Incompatible plugin deleted.");
                                } else {
                                    setErrorPrompt(L"Failed to delete incompatible plugin!");
                                    showErrorPrompt(L"OK");
                                    goto next_loop;
                                }
                            } else if (res == 2 || res == 255) { // Cancel or Back
                                goto next_loop;
                            }
                        }
                    }
                }

                {
                    std::wstring msg = L"Do you want to download " + toWstring(p.fileName) + L"?";
                    if (showDialogPrompt(msg.c_str(), L"Yes", L"No") == 0) {
                        std::string fullPath = posixPath;
                        if (fullPath.back() != '/') fullPath += "/";
                        fullPath += p.fileName;
                        if (downloadFile(p.downloadPath, fullPath)) {
                            showSuccessPrompt(L"Plugin downloaded successfully!");
                            changed = true;
                        }
                    }
                }
                next_loop:
                break;
            }
            sleep_for(50ms);
        }
    }
}

static bool syncPlugins(const std::string& sourcePosixPath) {
    std::string slcPosix = convertToPosixPath("/vol/storage_slc/sys/hax/ios_plugins");
    std::string sdPosix = convertToPosixPath("/vol/external01/wiiu/ios_plugins");
    std::string destPosixPath;
    std::string sourceFwImg;
    std::string destFwImg;

    if (sourcePosixPath == slcPosix) {
        destPosixPath = sdPosix;
        sourceFwImg = convertToPosixPath("/vol/storage_slc/sys/hax/fw.img");
        destFwImg = convertToPosixPath("/vol/external01/fw.img");
    } else if (sourcePosixPath == sdPosix) {
        destPosixPath = slcPosix;
        sourceFwImg = convertToPosixPath("/vol/external01/fw.img");
        destFwImg = convertToPosixPath("/vol/storage_slc/sys/hax/fw.img");
    } else {
        return false;
    }

    std::wstring msg = L"Do you want to sync all plugins from\n" + toWstring(sourcePosixPath) + L"\nto\n" + toWstring(destPosixPath) + L"?\nExisting plugins will be overwritten and others deleted.";
    if (showDialogPrompt(msg.c_str(), L"Yes", L"No") != 0) return false;

    bool copyFwImg = false;
    if (fileExist(sourceFwImg.c_str())) {
        msg = L"Do you also want to copy the minute (fw.img)?\nSource: " + toWstring(sourceFwImg) + L"\nDestination: " + toWstring(destFwImg);
        if (showDialogPrompt(msg.c_str(), L"Yes", L"No") == 0) {
            copyFwImg = true;
        }
    }

    WHBLogFreetypeStartScreen();
    WHBLogFreetypePrint(L"Syncing plugins...");
    WHBLogFreetypeDrawScreen();

    // Ensure destination directory exists
    if (!dirExist(destPosixPath.c_str())) {
        try {
            std::filesystem::create_directories(destPosixPath);
        } catch (...) {
            setErrorPrompt(L"Failed to create destination directory!");
            showErrorPrompt(L"OK");
            return false;
        }
    }

    // 1. Copy plugins from source to destination
    DIR* dir = opendir(sourcePosixPath.c_str());
    std::vector<std::string> sourceFiles;
    if (dir) {
        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr) {
            if (ent->d_type == DT_REG) {
                std::string fileName = ent->d_name;
                sourceFiles.push_back(fileName);
                std::string srcFile = sourcePosixPath;
                if (srcFile.back() != '/') srcFile += "/";
                srcFile += fileName;
                std::string destFile = destPosixPath;
                if (destFile.back() != '/') destFile += "/";
                destFile += fileName;

                WHBLogFreetypePrintf(L"Copying %S...", toWstring(fileName).c_str());
                WHBLogFreetypeDrawScreen();
                if (!copyFile(srcFile, destFile)) {
                     WHBLogFreetypePrintf(L"Failed to copy %S", toWstring(fileName).c_str());
                     WHBLogFreetypeDrawScreen();
                }
            }
        }
        closedir(dir);
    }

    // 2. Delete plugins in destination that are not in source
    dir = opendir(destPosixPath.c_str());
    if (dir) {
        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr) {
            if (ent->d_type == DT_REG) {
                std::string fileName = ent->d_name;
                if (std::find(sourceFiles.begin(), sourceFiles.end(), fileName) == sourceFiles.end()) {
                    WHBLogFreetypePrintf(L"Deleting %S...", toWstring(fileName).c_str());
                    WHBLogFreetypeDrawScreen();
                    std::string destFile = destPosixPath;
                    if (destFile.back() != '/') destFile += "/";
                    destFile += fileName;
                    remove(destFile.c_str());
                }
            }
        }
        closedir(dir);
    }

    // 3. Copy fw.img if requested
    if (copyFwImg) {
        WHBLogFreetypePrint(L"Copying fw.img...");
        WHBLogFreetypeDrawScreen();
        if (!copyFile(sourceFwImg, destFwImg)) {
            WHBLogFreetypePrint(L"Failed to copy fw.img!");
            WHBLogFreetypeDrawScreen();
            sleep_for(2s);
        }
    }

    showSuccessPrompt(L"Sync completed!");
    return true;
}

static bool managePlugins(std::string posixPath) {
    uint8_t selectedOption = 0;
    bool refreshList = true;
    bool changed = false;
    std::vector<std::string> plugins;

    std::string slcPosix = convertToPosixPath("/vol/storage_slc/sys/hax/ios_plugins");
    std::string sdPosix = convertToPosixPath("/vol/external01/wiiu/ios_plugins");
    bool isStandardPath = (posixPath == slcPosix || posixPath == sdPosix);

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
            const auto& cachedPluginList = getCachedPluginList();
            for (size_t i = 0; i < plugins.size(); i++) {
                std::wstring fileName = toWstring(plugins[i]);
                std::wstring shortDesc = L"";
                if (!cachedPluginList.empty()) {
                    for (const auto& p : cachedPluginList) {
                        if (p.fileName == plugins[i]) {
                            shortDesc = toWstring(p.shortDescription);
                            break;
                        }
                    }
                }
                if (shortDesc.empty()) {
                    WHBLogFreetypePrintf(L"%C %S", OPTION(i), fileName.c_str());
                } else {
                    if (fileName.length() < 16) fileName.append(16 - fileName.length(), L' ');
                    fileName += L" ";
                    WHBLogFreetypePrintf(L"%C %S %S", OPTION(i), fileName.c_str(), shortDesc.c_str());
                }
            }
        }

        WHBLogFreetypeScreenPrintBottom(L"===============================");
        if (isStandardPath) {
            WHBLogFreetypeScreenPrintBottom(L"\uE000 Select \uE001 Back \uE002 Delete \uE003 Get plugin \ue045 Sync All");
        } else {
            WHBLogFreetypeScreenPrintBottom(L"\uE000 Select \uE001 Back \uE002 Delete \uE003 Get plugin");
        }
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
                return changed;
            }
            if (pressedY()) {
                if (browsePlugins(posixPath)) {
                    refreshList = true;
                    changed = true;
                }
                break;
            }
            if (pressedStart() && isStandardPath) {
                if (syncPlugins(posixPath)) {
                    refreshList = true;
                    changed = true;
                    break;
                }
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
                            changed = true;
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

void checkForUpdates() {
    std::string slcPosix = convertToPosixPath("/vol/storage_slc/sys/hax/ios_plugins");
    std::string sdPosix = "fs:/vol/external01/wiiu/ios_plugins";
    std::string currentPosix = getStroopwafelPluginPosixPath();

    std::string targetPosix = "";
    std::string minutePath = "";

    if (currentPosix == slcPosix) {
        targetPosix = slcPosix;
        minutePath = convertToPosixPath("/vol/storage_slc/sys/hax/fw.img");
    } else if (currentPosix == sdPosix || currentPosix == "fs:/vol/external01/wiiu/ios_plugins/") {
        targetPosix = sdPosix;
        minutePath = "fs:/vol/external01/fw.img";
    } else {
        uint8_t choice = showDialogPrompt(L"Which location do you want to check for updates?", L"SD", L"SLC", L"Cancel");
        if (choice == 0) {
            targetPosix = sdPosix;
            minutePath = "fs:/vol/external01/fw.img";
        } else if (choice == 1) {
            if (!checkSystemAccess()) return;
            targetPosix = slcPosix;
            minutePath = convertToPosixPath("/vol/storage_slc/sys/hax/fw.img");
        } else {
            return;
        }
    }

    WHBLogFreetypeStartScreen();
    WHBLogFreetypePrint(L"Checking for updates...");
    WHBLogFreetypeDrawScreen();

    if (!fetchPluginList(true)) {
        showErrorPrompt(L"OK");
        return;
    }

    struct OutdatedFile {
        std::string fileName;
        std::string repo;
        std::string downloadUrl;
        std::string localPath;
    };
    std::vector<OutdatedFile> outdatedFiles;
    std::map<std::string, std::string> repoCache;

    auto getCachedResponse = [&](const std::string& repo) -> std::string {
        if (repoCache.find(repo) == repoCache.end()) {
            std::string response;
            if (downloadToBuffer("https://api.github.com/repos/" + repo + "/releases/latest", response)) {
                repoCache[repo] = response;
            } else {
                repoCache[repo] = "";
            }
        }
        return repoCache[repo];
    };

    // Check plugins
    DIR* dir = opendir(targetPosix.c_str());
    if (dir) {
        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr) {
            if (ent->d_type == DT_REG) {
                std::string fileName = ent->d_name;
                for (const auto& p : getCachedPluginList()) {
                    if (p.fileName == fileName) {
                        WHBLogFreetypePrintf(L"Checking %S...", toWstring(fileName).c_str());
                        WHBLogFreetypeDrawScreen();

                        std::string repo = getRepoFromUrl(p.downloadPath);
                        if (repo.empty()) {
                            WHBLogFreetypePrintf(L"No repo for %S", toWstring(fileName).c_str());
                            WHBLogFreetypeDrawScreen();
                            continue;
                        }

                        std::string response = getCachedResponse(repo);
                        if (response.empty()) {
                            WHBLogFreetypePrintf(L"No API response for %S", toWstring(repo).c_str());
                            WHBLogFreetypeDrawScreen();
                            continue;
                        }

                        std::string remoteHash = getDigestFromResponse(response, fileName);
                        if (remoteHash.empty()) {
                            WHBLogFreetypePrintf(L"No hash found for %S", toWstring(fileName).c_str());
                            WHBLogFreetypeDrawScreen();
                            continue;
                        }

                        std::string fullPath = targetPosix;
                        if (fullPath.back() != '/') fullPath += "/";
                        fullPath += fileName;

                        std::string localHash = calculateSHA256(fullPath);
                        WHBLogFreetypePrintf(L"L: %S", toWstring(localHash.substr(0, 8)).c_str());
                        WHBLogFreetypePrintf(L"R: %S", toWstring(remoteHash.substr(0, 8)).c_str());
                        WHBLogFreetypeDrawScreen();

                        if (localHash != remoteHash) {
                            outdatedFiles.push_back({fileName, repo, p.downloadPath, fullPath});
                        }
                        break;
                    }
                }
            }
        }
        closedir(dir);
    }

    // Check minute
    if (fileExist(minutePath.c_str())) {
        WHBLogFreetypePrint(L"Checking minute (fw.img)...");
        WHBLogFreetypeDrawScreen();

        std::string repo = "StroopwafelCFW/minute_minute";
        std::string response = getCachedResponse(repo);
        if (!response.empty()) {
            std::string hashFastboot = getDigestFromResponse(response, "fw_fastboot.img");
            std::string hashFull = getDigestFromResponse(response, "fw.img");

            // If we found at least one hash on GitHub, perform the check.
            // If both are empty, we assume up to date (no digest available).
            if (!hashFastboot.empty() || !hashFull.empty()) {
                std::string localHash = calculateSHA256(minutePath);
                WHBLogFreetypePrintf(L"L: %S", localHash.empty() ? L"EMPTY" : toWstring(localHash.substr(0, 8)).c_str());
                if (!hashFastboot.empty()) WHBLogFreetypePrintf(L"RF: %S", toWstring(hashFastboot.substr(0, 8)).c_str());
                if (!hashFull.empty()) WHBLogFreetypePrintf(L"RM: %S", toWstring(hashFull.substr(0, 8)).c_str());
                WHBLogFreetypeDrawScreen();

                bool upToDate = false;
                if (!hashFastboot.empty() && localHash == hashFastboot) {
                    upToDate = true;
                } else if (!hashFull.empty() && localHash == hashFull) {
                    upToDate = true;
                }

                if (!upToDate) {
                    WHBLogFreetypePrint(L"Minute is outdated!");
                    WHBLogFreetypeDrawScreen();
                    std::string downloadUrl = "https://github.com/StroopwafelCFW/minute_minute/releases/latest/download/fw_fastboot.img";
                    outdatedFiles.push_back({"fw.img", repo, downloadUrl, minutePath});
                }
            } else {
                WHBLogFreetypePrint(L"No hashes found for minute");
                WHBLogFreetypeDrawScreen();
            }
        }
    }

    if (outdatedFiles.empty()) {
        showSuccessPrompt(L"All files are up to date!");
    } else {
        std::wstring msg = L"Updates available for:\n";
        for (const auto& f : outdatedFiles) {
            msg += L"- " + toWstring(f.fileName) + L"\n";
        }
        msg += L"\nDo you want to update all?";

        if (showDialogPrompt(msg.c_str(), L"Update All", L"Cancel") == 0) {
            bool allSuccess = true;
            for (const auto& f : outdatedFiles) {
                if (!downloadFile(f.downloadUrl, f.localPath)) {
                    allSuccess = false;
                }
            }
            if (allSuccess) {
                showSuccessPrompt(L"All files updated successfully!");
                setShutdownPending(true);
                showDialogPrompt(L"Updates applied.\nYour console will reboot when you exit.", L"OK");
            } else {
                showErrorPrompt(L"OK");
            }
        }
    }
}

void showPluginManager() {
    bool anyChanged = false;
    std::string slcPosix = convertToPosixPath("/vol/storage_slc/sys/hax/ios_plugins");
    std::string sdPosix = convertToPosixPath("/vol/external01/wiiu/ios_plugins");
    std::string currentPosix = getStroopwafelPluginPosixPath();

    std::vector<std::pair<std::wstring, std::string>> options;
    options.push_back({L"SLC Plugins (/sys/hax/ios_plugins)", slcPosix});
    options.push_back({L"SD Plugins (/wiiu/ios_plugins)", sdPosix});

    uint8_t selectedOption = 0;
    if (!currentPosix.empty()) {
        if (currentPosix == slcPosix) {
            selectedOption = 0;
        } else if (currentPosix == sdPosix) {
            selectedOption = 1;
        } else {
            options.push_back({L"Current Plugins (" + toWstring(currentPosix) + L")", currentPosix});
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
                bool slcSelected = (options[selectedOption].second == slcPosix);
                if (slcSelected && !checkSystemAccess()) {
                    break;
                }
                if (managePlugins(options[selectedOption].second)) {
                    anyChanged = true;
                }
                break;
            }
            if (pressedBack()) {
                if (anyChanged) {
                    setShutdownPending(true);
                    if (showDialogPrompt(L"You have changed plugins.\nDo you want to reboot now to apply changes?", L"Yes", L"No") == 0) {
                        return;
                    }
                }
                return;
            }
            sleep_for(50ms);
        }
    }
}
