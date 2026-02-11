#include "startup_checks.h"
#include "menu.h"
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

bool performStartupChecks() {
    bool wantsPartitionedStorage = false;
    bool usingUSB = false;

    WHBLogPrint("Performing startup checks...");
    WHBLogFreetypeDraw();

    if (getStroopwafelPluginPosixPath().find("storage_slc") != std::string::npos) {
        if (!checkSystemAccess(true)) return false;
    }
    WHBLogFreetypeDraw();

    // 1. SD Card accessibility check
    bool sdAccessible = (WHBMountSdCard() == 1);

    if (sdAccessible) {
        FSAClientHandle fsaHandle = FSAAddClient(NULL);
        if (fsaHandle >= 0) {
            FSADeviceInfo devInfo;
            if ((FSStatus)FSAGetDeviceInfo(fsaHandle, "/dev/sdcard01", &devInfo) == FS_STATUS_OK) {
                bool dummy = false;
                checkAndFixPartitionOrder(fsaHandle, "/dev/sdcard01", devInfo, dummy);
            }
            FSADelClient(fsaHandle);
        }
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
                    sdAccessible = true;
                }
            }

            if (!sdAccessible) {
                showDeviceInfoScreen(fsaHandle, "/dev/sdcard01", devInfo);
                if (showDialogPrompt(L"SD card cannot be accessed.\nDo you want to format it to use it on the Wii U?", L"Yes", L"No", nullptr, nullptr, 0, false) == 0) {
                    uint64_t totalSize = (uint64_t)devInfo.deviceSizeInSectors * devInfo.deviceSectorSize;
                    uint64_t twoGiB = 2ULL * 1024 * 1024 * 1024;

                    showDeviceInfoScreen(fsaHandle, "/dev/sdcard01", devInfo);
                    if (totalSize >= twoGiB && showDialogPrompt(L"Do you also want to use the SD card to install Wii U games to?", L"Yes", L"No", nullptr, nullptr, 0, false) == 0) {
                        wantsPartitionedStorage = true;
                        if (partitionDevice(fsaHandle, "/dev/sdcard01", devInfo)) {
                            WHBMountSdCard();
                            download5sdusb(true, true);
                        }
                    } else {
                        if (formatWholeDrive(fsaHandle, "/dev/sdcard01", devInfo)) {
                            WHBMountSdCard();
                        }
                    }
                }
            }
        } else {
            // SD doesn't exist, try USB
            if (showDialogPrompt(L"No SD card detected.\nDo you want to use a USB device instead?", L"Yes", L"No") == 0) {
                usingUSB = true;
                usbAsSd(true);

                if (waitForDevice(fsaHandle, L"USB device")) {
                    FSStatus res = (FSStatus)FSAGetDeviceInfo(fsaHandle, "/dev/sdcard01", &devInfo);
                    if (res == FS_STATUS_OK) {
                        bool dummy = false;
                        checkAndFixPartitionOrder(fsaHandle, "/dev/sdcard01", devInfo, dummy);
                    }

                    if (WHBMountSdCard() == 1) {
                        showDeviceInfoScreen(fsaHandle, "/dev/sdcard01", devInfo);
                        if (showDialogPrompt(L"USB device detected.\nDo you want to repartition it to store Wii U games on or keep as is?", L"Repartition", L"Keep as is", nullptr, nullptr, 0, false) == 0) {
                            wantsPartitionedStorage = true;
                            if (partitionDevice(fsaHandle, "/dev/sdcard01", devInfo)) {
                                WHBMountSdCard();
                            }
                        }
                    } else {
                        // Cannot be mounted
                        showDeviceInfoScreen(fsaHandle, "/dev/sdcard01", devInfo);
                        if (showDialogPrompt(L"USB device cannot be mounted.\nDo you want to use the full drive for homebrew or also store Wii U games on it?", L"Homebrew only", L"Homebrew + Games", nullptr, nullptr, 0, false) == 1) {
                            wantsPartitionedStorage = true;
                            if (partitionDevice(fsaHandle, "/dev/sdcard01", devInfo)) {
                                WHBMountSdCard();
                            }
                        } else {
                            if (formatWholeDrive(fsaHandle, "/dev/sdcard01", devInfo)) {
                                WHBMountSdCard();
                            }
                        }
                    }
                    download5upartsd(true);
                }
            }
        }
        if (fsaHandle >= 0) FSADelClient(fsaHandle);
    }

    // 2. Aroma check
    if ((WHBMountSdCard() == 1) && !dirExist("fs:/vol/external01/wiiu/environments/aroma")) {
        if (showDialogPrompt(L"Aroma is missing on your SD card.\nDo you want to download it now?", L"Yes", L"No") == 0) {
            downloadAroma();
        }
    }

    // 3. Stroopwafel check
    bool stroopAvailable = isStroopwafelAvailable();
    if (!stroopAvailable) {
        uint8_t choice = showDialogPrompt(L"Stroopwafel is missing or outdated.\nDo you want to download it?", L"Yes", L"No");
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

    // 4. ISFShax check
    bool isfshaxInstalled = isIsfshaxInstalled();
    if (!isfshaxInstalled) {
        uint8_t choice = showDialogPrompt(L"ISFShax is not detected.\nDo you want to install it?\nThis is required for Stroopwafel.", L"Yes", L"No");
        if (choice == 0) {
            if (downloadIsfshaxFiles()) {
                bootInstaller();
            }
        } else if (choice == 1 || choice == 255) {
            if (usingUSB || wantsPartitionedStorage) {
                showDialogPrompt(L"You chose not to setup ISFShax.\nNote that USB-as-SD and partitioned storage REQUIRE Stroopwafel/ISFShax to work!", L"OK");
            }
        }
    }

    return true;
}
