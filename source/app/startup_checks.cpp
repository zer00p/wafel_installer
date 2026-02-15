#include "startup_checks.h"
#include "menu.h"
#include "isfshax_menu.h"
#include "pluginmanager.h"
#include "partition_manager.h"
#include "navigation.h"
#include "filesystem.h"
#include "gui.h"
#include "cfw.h"
#include "fw_img_loader.h"
#include "download.h"
#include <whb/sdcard.h>
#include <dirent.h>
#include <algorithm>
#include <vector>
#include <string>

static void setupInaccessibleSdCard(FSAClientHandle fsaHandle, FSADeviceInfo& devInfo, bool& wantsPartitionedStorage) {
    handleSDUSBAction(fsaHandle, devInfo, wantsPartitionedStorage);
}

static void setupUsbStorage(FSAClientHandle fsaHandle, bool& wantsPartitionedStorage) {
    usbAsSd(true);

    FatMountGuard guard;
    if (waitForDevice(fsaHandle, L"USB device", guard)) {
        FSADeviceInfo devInfo;
        if ((FSStatus)FSAGetDeviceInfo(fsaHandle, "/dev/sdcard01", &devInfo) == FS_STATUS_OK) {
            handleUSBAsSDAction(fsaHandle, devInfo, wantsPartitionedStorage);
        }
    }
}

void performAromaCheck() {
    if ((WHBMountSdCard() == 1) && !dirExist("fs:/vol/external01/wiiu/environments/aroma")) {
        if (showDialogPrompt(L"Aroma is missing on your SD card.\nDo you want to download it now?", L"Yes", L"No") == 0) {
            downloadAroma();
        }
    }
}

void performStroopwafelCheck(bool wantsPartitionedStorage) {
    if (!isStroopwafelAvailable()) {
        uint8_t choice = showDialogPrompt(L"Stroopwafel is missing, outdated or not running\nDo you want to download it?", L"Yes", L"No");
        if (choice == 0) {
            bool toSD = false;
            if (wantsPartitionedStorage) {
                toSD = false; // Only SLC for SD emulation
            } else {
                toSD = (showDialogPrompt(L"Where do you want to download Stroopwafel?\nSD card is recommended.", L"SD Card", L"SLC") == 0);
            }
            downloadStroopwafelFiles(toSD);
        }
    }
}

void performIsfshaxCheck(bool usingUSB, bool wantsPartitionedStorage) {
    if (!isIsfshaxInstalled()) {
        uint8_t choice = showDialogPrompt(L"ISFShax is not detected.\nDo you want to install it?\nThis is required for Stroopwafel.", L"Yes", L"No");
        if (choice == 0) {
            installIsfshax(false, false);
        } else if (choice == 1 || choice == 255) {
            if (usingUSB || wantsPartitionedStorage) {
                showDialogPrompt(L"You chose not to setup ISFShax.\nNote that USB-as-SD and partitioned storage REQUIRE Stroopwafel/ISFShax to work!", L"OK");
            }
        }
    }
}

void performPostSetupChecks(bool usingUSB, bool wantsPartitionedStorage) {
    performAromaCheck();
    performStroopwafelCheck(wantsPartitionedStorage);
    performIsfshaxCheck(usingUSB, wantsPartitionedStorage);

    std::string pluginTarget = getStroopwafelPluginPosixPath();
    if (pluginTarget.empty()) return;

    if (usingUSB) {
        if (isSdEmulated()) {
            downloadUsbPartitionPlugin("5upartsd.ipx", pluginTarget);
        } else if (wantsPartitionedStorage) {
            downloadUsbPartitionPlugin("5usbpart.ipx", pluginTarget);
        }
    } else if (wantsPartitionedStorage) {
        downloadUsbPartitionPlugin("5sdusb.ipx", pluginTarget);
    }
}

bool performStartupChecks() {
    bool wantsPartitionedStorage = false;
    bool usingUSB = false;

    WHBLogPrint("Performing startup checks...");
    WHBLogFreetypeDraw();

    if (getStroopwafelPluginPosixPath().find("storage_slc") != std::string::npos) {
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
                    FatMountGuard guard;
                    // We block here just in case checkAndFixPartitionOrder triggers a repartition/format
                    guard.block();
                    checkAndFixPartitionOrder(fsaHandle, "/dev/sdcard01", devInfo, dummy);
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
                std::vector<std::wstring> options = { L"Retry SD", L"Use USB", L"Abort" };
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
