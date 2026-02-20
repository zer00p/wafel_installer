#include "isfshax_menu.h"
#include "menu.h"
#include "gui.h"
#include "cfw.h"
#include "fw_img_loader.h"
#include "download.h"
#include "filesystem.h"
#include <isfshax_cmd.h>
#include <vector>
#include <string>

using namespace std::chrono_literals;

bool confirmIsfshaxAction(const wchar_t* action, bool isUninstall = false) {
    std::wstring message = L"";
    if (isUninstall) {
        message += L"WARNING: Before Uninstalling ISFShax make sure the console boots correctly using\n"
                   L"the 'Patch ISFShax and boot IOS (slc)' option in minute.\n"
                   L"If your console can't boot correctly, uninstalling ISFShax will BRICK the console!!!\n\n";
    }

    message += L"WARNING: You are about to make modifications to the console.\n"
                           L"This software comes with ABSOLUTELY NO WARRANTY!\n"
                           L"You are choosing to use this at your own risk.\n"
                           L"The author(s) will not be held liable for any damage.\n\n";

    message += L"Do you want to proceed with ";
    message += action;
    message += L"?";

    return showDialogPrompt(message.c_str(), L"Yes", L"No", nullptr, nullptr, 1) == 0;
}

void installIsfshax(bool uninstall, bool manual) {
    // For automated install, proactively download latest files
    if (!uninstall && !manual) {
        if (!downloadIsfshaxFiles()) return;
    }

    // For options 0, 1, 2 we need the installer file
    std::string fwImgPath = convertToPosixPath("/vol/storage_slc/sys/hax/installer/fw.img");
    if (!fileExist(fwImgPath.c_str())) {
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
            std::string slcTmpDir = convertToPosixPath("/vol/storage_slc/sys/tmp");
            std::string slcInstallerDir = convertToPosixPath("/vol/storage_slc/sys/hax/installer");
            std::string slcHaxDir = convertToPosixPath("/vol/storage_slc/sys/hax");

            std::string srcFwImg = convertToPosixPath("/vol/storage_slc/sys/hax/installer/fw.img");
            std::string destFwImg = slcTmpDir + "/fw.img";

            if (!dirExist(slcTmpDir.c_str())) {
                mkdir(slcTmpDir.c_str(), 0755);
            }

            if (moveFile(srcFwImg.c_str(), destFwImg.c_str())) {
                deleteDirContent(slcInstallerDir.c_str());
                removeDir(slcInstallerDir.c_str());
                if (isDirEmpty(slcHaxDir.c_str())) {
                    removeDir(slcHaxDir.c_str());
                }
                loadFwImg("/vol/system/tmp/fw.img", ISFSHAX_CMD_UNINSTALL, (uint32_t)(ISFSHAX_CMD_POST_REBOOT) << 30 | ISFSHAX_CMD_SOURCE_SLC);
            } else {
                setErrorPrompt(L"Failed to move installer to /sys/tmp!");
                showErrorPrompt(L"OK");
            }
        }
        return;
    }


    if (confirmIsfshaxAction(L"Install")) {
        loadFwImg("/vol/system/hax/installer/fw.img", ISFSHAX_CMD_INSTALL, (uint32_t)(ISFSHAX_CMD_POST_REBOOT) << 30 | ISFSHAX_CMD_SOURCE_SLC);
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
