#include "startup_checks.h"
#include "menu.h"
#include "isfshax_menu.h"
#include "pluginmanager.h"
#include "partition_manager.h"
#include "navigation.h"
#include "filesystem.h"
#include "common_paths.h"
#include "gui.h"
#include "cfw.h"
#include "fw_img_loader.h"
#include "download.h"
#include <whb/sdcard.h>
#include <dirent.h>
#include <algorithm>
#include <vector>
#include <string>
#include <chrono>

using namespace std::chrono_literals;

static void setupInaccessibleSdCard(FSAClientHandle fsaHandle, FSADeviceInfo& devInfo, bool& wantsPartitionedStorage) {
    uint64_t totalSize = (uint64_t)devInfo.deviceSizeInSectors * devInfo.deviceSectorSize;
    uint64_t twoGiB = 2ULL * 1024 * 1024 * 1024;

    showDeviceInfoScreen(fsaHandle, "/dev/sdcard01", devInfo);
    
    int choice = -1;
    if (totalSize >= twoGiB) {
        choice = showDialogPrompt(L"The SD card isn't formatted for the Wii U.\nDo you want to format it for homebrew or also store Wii U games on it?", L"Homebrew only", L"Homebrew + Games", L"Cancel", nullptr, 0, false);
    } else {
        uint8_t c = showDialogPrompt(L"The SD card isn't formatted for the Wii U.\nDo you want to format it to use it on the Wii U?", L"Format (Homebrew only)", L"Cancel", nullptr, nullptr, 0, false);
        choice = (c == 0) ? 0 : 2;
    }

    if (choice == 0 || choice == 1) {
        FatMountGuard guard;
        guard.block();

        if (choice == 1) {
            wantsPartitionedStorage = true;
            if (partitionDevice(fsaHandle, "/dev/sdcard01", devInfo)) {
                guard.unblock();
                WHBMountSdCard();
            }
        } else {
            if (formatWholeDrive(fsaHandle, "/dev/sdcard01", devInfo)) {
                guard.unblock();
                WHBMountSdCard();
            }
        }
    }
}

static void setupUsbStorage(FSAClientHandle fsaHandle, bool& wantsPartitionedStorage) {
    usbAsSd(true);

    FatMountGuard guard;
    if (waitForDevice(fsaHandle, L"USB device", guard)) {
        FSADeviceInfo devInfo;
        if ((FSStatus)FSAGetDeviceInfo(fsaHandle, "/dev/sdcard01", &devInfo) == FS_STATUS_OK) {
            bool dummy = false;
            checkAndFixPartitionOrder(fsaHandle, "/dev/sdcard01", devInfo, dummy);

            guard.unblock();
            if (WHBMountSdCard() == 1) {
                guard.block();
                if (handlePartitionActionMenu(fsaHandle, devInfo, L"SD card", false)) {
                    guard.unblock();
                    WHBMountSdCard();
                }
            } else {
                // Cannot be mounted
                showDeviceInfoScreen(fsaHandle, "/dev/sdcard01", devInfo);
                uint8_t usbChoice = showDialogPrompt(L"The USB device isn't formatted for Homebrew.\nDo you want to format it for homebrew or also store Wii U games on it?", L"Homebrew only", L"Homebrew + Games", L"Cancel", nullptr, 0, false);
                if (usbChoice == 1) {
                    wantsPartitionedStorage = true;
                    guard.block();
                    if (partitionDevice(fsaHandle, "/dev/sdcard01", devInfo)) {
                        guard.unblock();
                        WHBMountSdCard();
                    }
                } else if (usbChoice == 0) {
                    guard.block();
                    if (formatWholeDrive(fsaHandle, "/dev/sdcard01", devInfo)) {
                        guard.unblock();
                        WHBMountSdCard();
                    }
                }
            }
        }
    }
}

bool performAromaCheck() {
    if ((WHBMountSdCard() == 1) && !dirExist(Paths::SdAromaDir)) {
        return askAndDownloadAroma();
    }
    return true;
}

bool performStroopwafelCheck(bool& isInstalled) {
    bool filesExist = true;
    std::string path = getStroopwafelPluginPath();
    if (path.empty() || !dirExist(path)) {
        filesExist = false;
    }

    while (!isStroopwafelAvailable() || !filesExist) {
        uint8_t choice = showDialogPrompt(L"Stroopwafel is missing, outdated or not running\nDo you want to download stroopwafel by shinyquagsire23?", L"Yes", L"No");
        if (choice != 0) {
            isInstalled = false;
            return true;
        }

        if (installStroopwafel()) {
            isInstalled = true;
            return true;
        } else {
            uint8_t failChoice = showDialogPrompt(L"Stroopwafel installation failed.\nDo you want to retry?", L"Retry", L"Abort");
            if (failChoice == 0) continue;
            if (failChoice == 1) {
                return false;
            }
            return false;
        }
    }
    isInstalled = true;
    return true;
}

bool performIsfshaxCheck(bool usingUSB, bool wantsPartitionedStorage) {
    if (isIsfshaxInstalled())
        return true;
    // TODO: add message for SDUSB / USB Partition
    uint8_t choice = showDialogPrompt(L"ISFShax is not detected.\nDo you want to install ISFShax by rw_r_r_0644?\nThis is required for Stroopwafel.", L"Yes", L"No");
    if (choice == 0) {
        installIsfshax(false, false);
    } else if (choice == 1 || choice == 255) {
        if (usingUSB || wantsPartitionedStorage) {
            showDialogPrompt(L"You chose not to setup ISFShax.\nNote that USB-as-SD and partitioned storage REQUIRE Stroopwafel and ISFShax to work!", L"OK");
        }
    }
    return false;
}

bool performPostSetupChecks(bool usingUSB, bool sdUsb) {
    if (!performAromaCheck()) return false;

    bool stroopwafelInstalled = false;
    if (!performStroopwafelCheck(stroopwafelInstalled)) return false;

    bool ret = true;
    if (stroopwafelInstalled) {
        if(usingUSB){
            ret = downloadUsbPartitionPlugin(true);
        }
        if(sdUsb){
            ret =  downloadPlugin("5sdusb.ipx");
        }
    }
    ret |= performIsfshaxCheck(usingUSB, sdUsb);
    if (abortChecks) return false;

    if (sdUsb || usingUSB) {
        showSuccessPrompt(WFS_FORMAT_REMINDER);
    }

    return ret;
}

bool performStartupChecks() {
    bool wantsPartitionedStorage = false;
    bool usingUSB = false;

    WHBLogPrint("Performing startup checks...");
    WHBLogFreetypeDraw();

    if (getStroopwafelPluginPath().find("storage_slc") != std::string::npos) {
        if (!checkSystemAccess(true)) return false;
    }
    WHBLogFreetypeDraw();

    while (true) {
        // 1. SD Card accessibility check
        if (WHBMountSdCard() == 1) {
            FSAClientHandle fsaHandle = FSAAddClient(NULL);
            if (fsaHandle >= 0) {
                FSADeviceInfo devInfo;
                if ((FSStatus)FSAGetDeviceInfo(fsaHandle, "/dev/sdcard01", &devInfo) == FS_STATUS_OK) {
                    bool dummy = false;
                    checkAndFixPartitionOrder(fsaHandle, "/dev/sdcard01", devInfo, dummy);

                    if (!dirExist(Paths::SdRoot + "/wiiu")) {
                        if (checkSdCardPartitioning(fsaHandle, devInfo)) {
                            wantsPartitionedStorage = true;
                        }
                    } else {
                        uint8_t* mbr = (uint8_t*)memalign(0x40, devInfo.deviceSectorSize);
                        if (mbr) {
                            MbrPartitionInfo info;
                            if (getMbrPartitionInfo(fsaHandle, "/dev/sdcard01", devInfo, mbr, info)) {
                                if (info.hasWfs) {
                                    std::string pluginPath = getStroopwafelPluginPath();
                                    bool alreadyConfigured = false;
                                    if (!pluginPath.empty()) {
                                        alreadyConfigured = fileExist(pluginPath + "/5upartsd.ipx") ||
                                                            fileExist(pluginPath + "/5sdusb.ipx");
                                    } else {
                                        alreadyConfigured = fileExist(Paths::SdPluginsDir + "/5upartsd.ipx") ||
                                                            fileExist(Paths::SlcPluginsDir + "/5upartsd.ipx") ||
                                                            fileExist(Paths::SdPluginsDir + "/5sdusb.ipx") ||
                                                            fileExist(Paths::SlcPluginsDir + "/5sdusb.ipx");
                                    }

                                    if (!alreadyConfigured && showDialogPrompt(L"A Wii U or NTFS partition was detected on the SD card.\nDo you want to use it to store Wii U games?", L"Yes", L"No") == 0) {
                                        wantsPartitionedStorage = true;
                                    }
                                }
                            }
                            free(mbr);
                        }
                    }
                }
                FSADelClient(fsaHandle);
            }
            break;
        } else {
            WHBLogPrint("SD card not accessible.");
            WHBLogFreetypeDraw();
            FSAClientHandle fsaHandle = FSAAddClient(NULL);
            FSADeviceInfo devInfo;
            bool sdExists = (fsaHandle >= 0) && ((FSStatus)FSAGetDeviceInfo(fsaHandle, "/dev/sdcard01", &devInfo) == FS_STATUS_OK);

            if (sdExists) {
                bool dummy = false;
                if (checkAndFixPartitionOrder(fsaHandle, "/dev/sdcard01", devInfo, dummy)) {
                    if (WHBMountSdCard() == 1) {
                        // All good
                    }
                }

                if (WHBMountSdCard() != 1) {
                    setupInaccessibleSdCard(fsaHandle, devInfo, wantsPartitionedStorage);
                }
                if (fsaHandle >= 0) FSADelClient(fsaHandle);
                break;
            } else {
                // SD doesn't exist, try USB
                std::vector<std::wstring> options = { L"Retry SD", L"Use USB (do NOT connect it just yet)", L"Abort" };
                uint8_t choice = showDialogPrompt(L"No SD card detected.", options, 0);

                if (choice == 0) {
                    WHBLogFreetypeStartScreen();
                    WHBLogPrint("Checking for SD card...");
                    WHBLogFreetypeDraw();
                    sleep_for(1s);
                    if (fsaHandle >= 0) FSADelClient(fsaHandle);
                    continue;
                } else if (choice == 1) {
                    usingUSB = true;
                    setupUsbStorage(fsaHandle, wantsPartitionedStorage);
                    if (fsaHandle >= 0) FSADelClient(fsaHandle);
                    break;
                } else {
                    if (fsaHandle >= 0) FSADelClient(fsaHandle);
                    return true;
                }
            }
        }
    }

    performPostSetupChecks(usingUSB, wantsPartitionedStorage);

    return true;
}
