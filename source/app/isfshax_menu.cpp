#include "isfshax_menu.h"
#include "menu.h"
#include "gui.h"
#include "cfw.h"
#include "fw_img_loader.h"
#include "download.h"
#include "filesystem.h"
#include "common_paths.h"
#include "urls.h"
#include "common.h"
#include "pluginmanager.h"
#include <mbedtls/sha1.h>
#include <isfshax_cmd.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>

using namespace std::chrono_literals;

bool confirmIsfshaxAction(const wchar_t* action, bool isUninstall = false) {
    std::wstring message = L"";
    if (isUninstall && isStroopwafelAvailable()) {
        message += L"WARNING: Before Uninstalling ISFShax make sure the console\n"
                   L"boots correctly using the 'Patch ISFShax and boot IOS (slc)'\n"
                   L"option in minute. If your console can't boot correctly,\n"
                   L"uninstalling ISFShax will BRICK the console!!!\n \n";
    }

    message += L"WARNING: You are about to make modifications to the console.\n"
                           L"This software comes with ABSOLUTELY NO WARRANTY!\n"
                           L"You are choosing to use this at your own risk.\n"
                           L"The author(s) will not be held liable for any damage.\n \n";

    message += L"Do you want to proceed with ";
    message += action;
    message += L"?";

    return showDialogPrompt(message.c_str(), L"Yes", L"No", nullptr, nullptr, 1) == 0;
}

std::vector<unsigned char> calculateSHA1(const std::string& path) {
    FILE* file = fileFopen(path.c_str(), "rb");
    if (!file) {
        return {};
    }

    mbedtls_sha1_context ctx;
    mbedtls_sha1_init(&ctx);
    mbedtls_sha1_starts(&ctx);

    unsigned char buffer[4096];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        mbedtls_sha1_update(&ctx, buffer, bytesRead);
    }

    unsigned char hash[20];
    mbedtls_sha1_finish(&ctx, hash);
    mbedtls_sha1_free(&ctx);
    fclose(file);

    return std::vector<unsigned char>(hash, hash + 20);
}

bool verifySuperblock() {
    std::wstring sblockFilename = toWstring(std::filesystem::path(Paths::SlcInstallerSblockImg).filename().string());
    std::wstring shaFilename = toWstring(std::filesystem::path(Paths::SlcInstallerSblockSha).filename().string());

    if (!fileExist(Paths::SlcInstallerSblockImg)) {
        setErrorPrompt((L"Superblock image (" + sblockFilename + L") is missing!").c_str());
        showErrorPrompt(L"OK");
        return false;
    }
    if (!fileExist(Paths::SlcInstallerSblockSha)) {
        setErrorPrompt((L"Superblock SHA file (" + shaFilename + L") is missing!").c_str());
        showErrorPrompt(L"OK");
        return false;
    }

    std::vector<unsigned char> expectedSha;
    FILE* shaFile = fileFopen(Paths::SlcInstallerSblockSha, "rb");
    if (shaFile) {
        unsigned char hashBuf[20];
        if (fread(hashBuf, 1, 20, shaFile) == 20) {
            expectedSha.assign(hashBuf, hashBuf + 20);
        }
        fclose(shaFile);
    }

    if (expectedSha.empty()) {
        setErrorPrompt((L"Failed to read SHA-1 hash from " + shaFilename + L"!").c_str());
        showErrorPrompt(L"OK");
        return false;
    }

    std::vector<unsigned char> actualSha = calculateSHA1(Paths::SlcInstallerSblockImg);
    if (actualSha.empty()) {
        setErrorPrompt((L"Failed to calculate SHA-1 of " + sblockFilename + L"!").c_str());
        showErrorPrompt(L"OK");
        return false;
    }

    if (actualSha != expectedSha) {
        auto toHex = [](const std::vector<unsigned char>& v) {
            std::wstringstream wss;
            for (size_t i = 0; i < std::min(v.size(), (size_t)8); i++) {
                wss << std::hex << std::setw(2) << std::setfill(L'0') << (int)v[i];
            }
            return wss.str();
        };
        setErrorPrompt(L"Superblock hash mismatch!\nExpected: " + toHex(expectedSha) + L"...\nActual: " + toHex(actualSha) + L"...");
        showErrorPrompt(L"OK");
        return false;
    }

    return true;
}

void installIsfshax(bool uninstall, bool manual) {
    if (uninstall && isRedNAND()) {
        showDialogPrompt(L"redNAND is detected.\nISFShax is required for redNAND.\nUninstallation is not possible.", L"OK");
        return;
    }

    if (uninstall && hasUnknownPlugins()) {
        return;
    }

    // For automated install, proactively download latest files
    if (!uninstall && !manual) {
        if (!downloadIsfshaxFiles()) return;
    }

    bool installer_exists = fileExist(Paths::SlcInstallerFwImg);

    // For we need the installer file, uninstall will download later
    if (!uninstall && !installer_exists) {
        uint8_t missingChoice = showDialogPrompt(L"The ISFShax installer (fw.img) is missing.", L"Download", L"Cancel");
        if (missingChoice == 0) {
            if (!downloadIsfshaxFiles()) return;
        } else {
            return;
        }
    }

    if (manual) {
        bootInstaller();
        return;
    }

    if(uninstall){
        if (confirmIsfshaxAction(L"Uninstall", true)) {
            if(installer_exists){
                if (moveFile(Paths::SlcInstallerFwImg, Paths::SystemTmpFwImg)) {
                    deleteDirContent(Paths::SlcInstallerDir);
                    removeDir(Paths::SlcInstallerDir);
                    if (isDirEmpty(Paths::SlcHaxDir)) {
                        removeDir(Paths::SlcHaxDir);
                    }
                } else {
                    setErrorPrompt(L"Failed to move installer to /sys/tmp!");
                    showErrorPrompt(L"OK");
                    return;
                }
            } else {
                if(!downloadFile(URLs::IsfshaxInstallerIosImg, Paths::SystemTmpFwImg)){
                    setErrorPrompt(L"Failed to download ISFShax installer to /sys/tmp!");
                    showErrorPrompt(L"OK");
                    return;
                }
            }
            loadFwImg(Paths::SystemTmpFwImg, ISFSHAX_CMD_UNINSTALL, (uint32_t)(ISFSHAX_CMD_POST_REBOOT) << 30 | ISFSHAX_CMD_SOURCE_SLC);
        }
        return;
    }


    if (confirmIsfshaxAction(L"Install")) {
        if (verifySuperblock()) {
            loadFwImg(Paths::SystemHaxInstallerFwImg, ISFSHAX_CMD_INSTALL, (uint32_t)(ISFSHAX_CMD_POST_REBOOT) << 30 | ISFSHAX_CMD_SOURCE_SLC);
        }
    }
}

void installIsfshaxMenu() {
    if (!checkSystemAccess()) return;

    std::vector<std::wstring> options = {
        L"Install (Automated)",
        L"Uninstall (Automated)",
        L"Expert (Manual)",
        L"Download latest files",
        L"Cancel"
    };

    uint8_t choice = showDialogPrompt(L"Select an option for the ISFShax installer:", options);
    if (choice == 4 || choice == 255) return;

    if (choice == 3) {
        if (downloadIsfshaxFiles()) {
            showSuccessPrompt(L"ISFShax files downloaded successfully!");
        }
        return;
    }

    installIsfshax(choice==1, choice==2);
}

void bootInstaller() {
    if (!checkSystemAccess()) return;

    std::string fwImgPath = Paths::SlcInstallerFwImg;
    if (!fileExist(fwImgPath)) {
        setErrorPrompt(L"ISFShax installer (fw.img) is missing!");
        showErrorPrompt(L"OK");
        return;
    }

    while (true) {
        sleep_for(1s);
        uint8_t choice = showDialogPrompt(L"The ISFShax installer is controlled with the buttons on the main console.\nPOWER: moves the curser\nEJECT: confirm\nPress A to launch into the ISFShax Installer", L"Continue", L"Cancel");
        if (choice == 0) {
            if (verifySuperblock()) {
                loadFwImg(Paths::SystemHaxInstallerFwImg);
                break;
            }
        } else {
            if (showDialogPrompt(L"Are you sure? ISFShax is required for stroopwafel", L"Yes, cancel", L"No, go back") == 0) {
                return;
            }
        }
    }
}
