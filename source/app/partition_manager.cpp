#include "partition_manager.h"
#include "menu.h"
#include "gui.h"
#include "filesystem.h"
#include "download.h"
#include "common.h"
#include "navigation.h"
#include "cfw.h"
#include "fw_img_loader.h"
#include <malloc.h>
#include <cstring>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <coreinit/ios.h>
#include <coreinit/filesystem_fsa.h>
#include <coreinit/time.h>
#include <mocha/mocha.h>
#include <mocha/fsa.h>
#include <whb/sdcard.h>

typedef struct __attribute__((packed)) {
    uint32_t unused;
    char device[0x280];
    char filesystem[8];
    uint32_t flags;
    uint32_t param_5;
    uint32_t param_6;
} FSAFormatRequest;

typedef struct __attribute__((aligned(0x40))) {
    union {
        uint8_t inbuf[0x520];
        FSAFormatRequest format;
    };
    uint8_t outbuf[0x293];
    uint8_t padding[0x30];
    uint32_t handle;
    uint32_t command;
    uint8_t unknown[0x3d];
} FSAIpcData;

static int32_t FSA_Format(FSAClientHandle handle, const char* device, const char* filesystem, uint32_t flags, uint32_t param_5, uint32_t param_6) {
    FSAIpcData* data = (FSAIpcData*)memalign(0x40, sizeof(FSAIpcData));
    if (!data) return -1;
    memset(data, 0, sizeof(FSAIpcData));

    data->handle = (uint32_t)handle;
    data->command = 0x69;

    strncpy(data->format.device, device, sizeof(data->format.device) - 1);
    strncpy(data->format.filesystem, filesystem, sizeof(data->format.filesystem) - 1);

    data->format.flags = flags;
    data->format.param_5 = param_5;
    data->format.param_6 = param_6;

    int32_t ret = IOS_Ioctl((int)handle, 0x69, data, 0x520, data->outbuf, 0x293);

    free(data);
    return ret;
}

void usbAsSd(bool enable){
    // Attach the USB device with SD type
    if(enable) {
        // Set device type to SD (0x6)
        Mocha_IOSUKernelWrite32(0x1077eda0, 0xe3a03006); // mov r3,#0x6
    } else {
        // Set device type to USB (0x11) (original original ins)
        Mocha_IOSUKernelWrite32(0x1077eda0, 0xe3a03011); // mov r3,#0x11
    }
}

static void setCustomFormatSize(uint32_t custom_size) {
    // skip cylinder alignment
    //Mocha_IOSUKernelWrite32(0x1078e4fc, 0xe1530003);

    // Remove 32GB check
    if(!custom_size) {
        Mocha_IOSUKernelWrite32(0x1078e354, 0xEA000075); // b 0x1078e530
    } else {
        // 1. Load the custom sector count from 0x1078e360 into R4
        // Opcode: LDR R4, [PC, #8] -> PC is 0x35C, Target is 0x360
        Mocha_IOSUKernelWrite32(0x1078e354, 0xE59F4010);

        // 2. Store R4 into the FSFAT_BlockCount struct
        // Opcode: STR R4, [R2, #0x14]
        Mocha_IOSUKernelWrite32(0x1078e358, 0xE5824014);

        // str r4, [sp, #0x88]  FStack_240.block_count_hi
        Mocha_IOSUKernelWrite32(0x1078e35c, 0xE58D4088);

        // 3. Jump to the FAT32 formatting logic (Original BLS target: 1078e530)
        // Math: 0x1078e530 - (0x1078e35c + 8) = 0x1CC -> 0x1CC / 4 = 0x73
        Mocha_IOSUKernelWrite32(0x1078e360, 0xEA000072); // b 0x1078e530

        // 4. THE DATA: Your total sector count read by LDR
        Mocha_IOSUKernelWrite32(0x1078e36c, custom_size);
    }

    // Patch SD Geometry Table
    // last entry:
    Mocha_IOSUKernelWrite32(0x1080bf20, 0xffffffff); //max size
}

static FSError rawRead(FSAClientHandle fsaHandle, const char* device, uint32_t sector, uint32_t count, void* buffer, uint32_t sectorSize) {
    IOSHandle handle = -1;
    FSError res = FSAEx_RawOpenEx(fsaHandle, device, &handle);
    if (res < 0) return res;

    res = FSAEx_RawReadEx(fsaHandle, buffer, sectorSize, count, sector, handle);
    FSAEx_RawCloseEx(fsaHandle, handle);
    return res;
}

static void write32LE(uint8_t* p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

static uint32_t read32LE(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static const char* getPartitionTypeName(uint8_t type) {
    switch (type) {
        case 0x01: return "FAT12";
        case 0x04: return "FAT16 <32M";
        case 0x06: return "FAT16";
        case 0x0B: return "FAT32";
        case 0x0C: return "FAT32 (LBA)";
        case 0x07: return "NTFS/exFAT/WFS";
        case 0x0E: return "FAT16 (LBA)";
        case 0x0F: return "Extended (LBA)";
        case 0x82: return "Linux Swap";
        case 0x83: return "Linux";
        case 0xEE: return "GPT/EFI";
        default: return "Unknown";
    }
}

static FSError rawWrite(FSAClientHandle fsaHandle, const char* device, uint32_t sector, uint32_t count, const void* buffer, uint32_t sectorSize) {
    IOSHandle handle = -1;
    FSError res = FSAEx_RawOpenEx(fsaHandle, device, &handle);
    if (res < 0) return res;

    res = FSAEx_RawWriteEx(fsaHandle, (void*)buffer, sectorSize, count, sector, handle);
    FSAEx_RawCloseEx(fsaHandle, handle);
    return res;
}

static FSError writeMbrSignature(FSAClientHandle fsaHandle, const char* device, uint32_t sector, uint32_t sectorSize) {
    uint8_t* buf = (uint8_t*)memalign(0x40, sectorSize);
    if (!buf) return (FSError)-1;
    memset(buf, 0, sectorSize);

    FSError res = rawRead(fsaHandle, device, sector, 1, buf, sectorSize);
    if ((FSStatus)res == FS_STATUS_OK) {
        buf[510] = 0x55;
        buf[511] = 0xAA;
        res = rawWrite(fsaHandle, device, sector, 1, buf, sectorSize);
    }
    free(buf);
    return res;
}

std::wstring getDeviceSummary(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo) {
    std::wstringstream ss;
    uint64_t totalSize = (uint64_t)deviceInfo.deviceSizeInSectors * deviceInfo.deviceSectorSize;
    double sizeMB = (double)totalSize / (1024.0 * 1024.0);
    double sizeGB = sizeMB / 1024.0;

    if (totalSize < 2ULL * 1024 * 1024 * 1024) {
        ss << L"Capacity: " << std::fixed << std::setprecision(0) << sizeMB << L" MB\n";
    } else {
        ss << L"Capacity: " << std::fixed << std::setprecision(2) << sizeGB << L" GB\n";
    }

    uint8_t* mbr = (uint8_t*)memalign(0x40, deviceInfo.deviceSectorSize);
    if (mbr) {
        if ((FSStatus)rawRead(fsaHandle, device, 0, 1, mbr, deviceInfo.deviceSectorSize) == FS_STATUS_OK) {
            if (mbr[510] == 0x55 && mbr[511] == 0xAA) {
                ss << L"Existing partitions:\n";
                for (int i = 0; i < 4; i++) {
                    uint8_t type = mbr[446 + i * 16 + 4];
                    if (type != 0) {
                        uint32_t sectors = read32LE(&mbr[446 + i * 16 + 12]);
                        double partSizeMB = (double)sectors * (double)deviceInfo.deviceSectorSize / (1024.0 * 1024.0);
                        double partSizeGB = partSizeMB / 1024.0;
                        ss << L"  P" << (i + 1) << L": " << toWstring(getPartitionTypeName(type));
                        ss << std::fixed << std::setprecision(2);
                        if (partSizeMB < 1024.0) {
                            ss << L" (" << partSizeMB << L" MB)\n";
                        } else {
                            ss << L" (" << partSizeGB << L" GB)\n";
                        }
                    }
                }
            } else {
                ss << L"No MBR found! Wii U Formatted?\n";
            }
        } else {
            ss << L"Failed to read MBR.\n";
        }
        free(mbr);
    }
    return ss.str();
}

bool formatWholeDrive(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo) {
    uint64_t totalSize = (uint64_t)deviceInfo.deviceSizeInSectors * deviceInfo.deviceSectorSize;
    uint64_t twoGiB = 2ULL * 1024 * 1024 * 1024;
    const char* fsType = (totalSize < twoGiB) ? "fat" : "fat"; // FAT16 is used by system automatically if < 2GiB and we pass "fat"

    if (showDialogPrompt(L"WARNING: This will format the whole device and DELETE ALL DATA on it.\nDo you want to continue?", L"Yes", L"No", nullptr, nullptr, 1) != 0) {
        return false;
    }

    while (true) {
        setCustomFormatSize(0);
        WHBLogFreetypePrintf(L"Formatting whole %ls...", toWstring(device).c_str());
        WHBLogFreetypeDraw();
        FSStatus status = (FSStatus)FSA_Format(fsaHandle, device, fsType, 0, 0, 0);
        if (status != FS_STATUS_OK) {
            WHBLogPrintf("Format failed (status: %d)!\n", status);
            WHBLogFreetypeDraw();
            setErrorPrompt(L"Failed to format device!");
            if (!showErrorPrompt(L"Cancel", true)) {
                return false;
            }
        } else {
            break;
        }
    }
    return true;
}

bool partitionDevice(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo) {
    double totalGB = (double)deviceInfo.deviceSizeInSectors * (double)deviceInfo.deviceSectorSize / (1024.0 * 1024.0 * 1024.0);
    int fatPercent = 80;
    if (totalGB < 1.0) fatPercent = 100;

    while(true) {
        double fatGB = totalGB * fatPercent / 100.0;
        double ntfsGB = totalGB * (100 - fatPercent) / 100.0;

        WHBLogFreetypeStartScreen();
        WHBLogFreetypePrint((L"Partition " + toWstring(device)).c_str());
        WHBLogFreetypePrint(L"===============================");
        WHBLogFreetypePrintf(L"FAT32: [ %d%% ] (%.2f GB)", fatPercent, fatGB);
        WHBLogFreetypePrintf(L"WFS:   [ %d%% ] (%.2f GB)", 100 - fatPercent, ntfsGB);
        WHBLogFreetypePrint(L"");
        WHBLogFreetypePrint(L"Use Left/Right to adjust (10% increments)");
        WHBLogFreetypePrint(L"Press A to confirm, B to cancel");
        WHBLogFreetypeScreenPrintBottom(L"===============================");
        WHBLogFreetypeDrawScreen();

        OSSleepTicks(OSMillisecondsToTicks(100));
        updateInputs();
        bool confirmed = false;
        bool cancelled = false;
        while(true) {
            updateInputs();
            if (navigatedLeft() && fatPercent > 10) {
                if (totalGB >= 1.0) {
                    if (totalGB * (fatPercent - 10) / 100.0 >= 1.0) {
                        fatPercent -= 10;
                    }
                }
                break;
            }
            if (navigatedRight() && fatPercent < 100) {
                fatPercent += 10;
                break;
            }
            if (pressedOk()) {
                confirmed = true;
                break;
            }
            if (pressedBack()) {
                cancelled = true;
                break;
            }
            OSSleepTicks(OSMillisecondsToTicks(50));
        }
        if (confirmed) break;
        if (cancelled) {
            return false;
        }
    }

    if (showDialogPrompt(L"WARNING: This will RE-PARTITION the whole device and DELETE ALL DATA on it.\nDo you want to continue?", L"Yes", L"No", nullptr, nullptr, 1) != 0) {
        return false;
    }

    uint32_t alignSectors = (64 * 1024 * 1024) / deviceInfo.deviceSectorSize;
    uint32_t p1_size = (uint32_t)(deviceInfo.deviceSizeInSectors * fatPercent / 100);

    while (true) {
        WHBLogPrint("Formatting FAT32 partition...");
        WHBLogFreetypeDraw();
        setCustomFormatSize(p1_size);
        FSStatus status = (FSStatus)FSA_Format(fsaHandle, device, "fat", 0, 0, 0);
        if (status != FS_STATUS_OK) {
            WHBLogPrintf("Format failed (status: %d)!\n", status);
            WHBLogFreetypeDraw();
            setErrorPrompt(L"Failed to format FAT32 partition!");
            if (!showErrorPrompt(L"Cancel", true)) {
                return false;
            }
        } else {
            break;
        }
    }

    uint8_t* mbr = (uint8_t*)memalign(0x40, deviceInfo.deviceSectorSize);
    if (!mbr) return false;

    // Read the new MBR created by FSA_Format
    if ((FSStatus)rawRead(fsaHandle, device, 0, 1, mbr, deviceInfo.deviceSectorSize) == FS_STATUS_OK) {
        uint32_t actual_p1_start = read32LE(&mbr[446 + 8]);
        uint32_t actual_p1_size = read32LE(&mbr[446 + 12]);

        uint32_t p2_start = ((actual_p1_start + actual_p1_size + alignSectors - 1) / alignSectors) * alignSectors;
        if (p2_start < deviceInfo.deviceSizeInSectors) {
            uint32_t p2_size = (uint32_t)(deviceInfo.deviceSizeInSectors - p2_start);

            uint8_t* pte2 = &mbr[446 + 3 * 16];
            pte2[4] = 0x07;
            write32LE(&pte2[8], p2_start);
            write32LE(&pte2[12], p2_size);

            WHBLogPrint("Adding second partition to MBR...");
            WHBLogFreetypeDraw();
            if ((FSStatus)rawWrite(fsaHandle, device, 0, 1, mbr, deviceInfo.deviceSectorSize) == FS_STATUS_OK) {
                writeMbrSignature(fsaHandle, device, p2_start, deviceInfo.deviceSectorSize);
            } else {
                setErrorPrompt(L"Failed to write second partition to MBR!");
                showErrorPrompt(L"OK");
                free(mbr);
                return false;
            }
        }
    }
    free(mbr);
    return true;
}

void formatAndPartitionMenu() {
    uint8_t deviceChoice = showDialogPrompt(L"Which device do you want to format?", L"SD", L"USB");
    if (deviceChoice == 255) return;
    bool use_usb = (deviceChoice == 1);
    const wchar_t* deviceName = use_usb ? L"USB drive" : L"SD card";

    usbAsSd(use_usb);

    WHBLogPrint("Opening /dev/fsa...");
    WHBLogFreetypeDraw();
    FSAClientHandle fsaHandle = FSAAddClient(NULL);
    if (fsaHandle < 0) {
        WHBLogPrintf("Failed to open /dev/fsa! Status: 0x%08X", fsaHandle);
        WHBLogFreetypeDraw();
        setErrorPrompt(L"Failed to open /dev/fsa!");
        showErrorPrompt(L"OK");
        return;
    }

    WHBLogFreetypePrintf(L"Unmounting %ls...", deviceName);
    WHBLogFreetypeDrawScreen();
    int status = WHBUnmountSdCard();
    if (status != 1) {
        WHBLogPrintf("Unmount failed (status: %d), ignoring...", status);
        WHBLogFreetypeDraw();
    }

    bool shouldDownloadAroma = false;

    while (true) {
        while (true) {
            uint8_t choice = showDialogPrompt(L"Remove all SD and USB Storage devices NOW!", L"OK", L"Cancel");
            if (choice != 0) {
                FSADelClient(fsaHandle);
                return;
            }

            FSADeviceInfo devInfo;
            if ((FSStatus)FSAGetDeviceInfo(fsaHandle, "/dev/sdcard01", &devInfo) != FS_STATUS_OK) {
                break;
            }
        }

        std::wstring pluginMsg = L"Plugin the " + std::wstring(deviceName) + L" you want to format";
        while (true) {
            WHBLogFreetypeStartScreen();
            WHBLogFreetypePrint(pluginMsg.c_str());
            WHBLogFreetypePrint(L"");
            WHBLogFreetypePrint(L"Waiting for device...");
            WHBLogFreetypePrint(L"Press B to cancel");
            WHBLogFreetypeDrawScreen();

            FSADeviceInfo devInfo;
            if ((FSStatus)FSAGetDeviceInfo(fsaHandle, "/dev/sdcard01", &devInfo) == FS_STATUS_OK) {
                break;
            }

            updateInputs();
            if (pressedBack()) {
                FSADelClient(fsaHandle);
                return;
            }
            OSSleepTicks(OSMillisecondsToTicks(100));
        }

        FSADeviceInfo deviceInfo;
        status = (int)FSAGetDeviceInfo(fsaHandle, "/dev/sdcard01", &deviceInfo);
        if ((FSStatus)status != FS_STATUS_OK) {
            WHBLogPrintf("FSAGetDeviceInfo failed: %d", status);
            WHBLogFreetypeDraw();
            setErrorPrompt(L"Failed to get device info!");
            showErrorPrompt(L"OK");
            FSADelClient(fsaHandle);
            return;
        }

        uint8_t* mbr = (uint8_t*)memalign(0x40, deviceInfo.deviceSectorSize);
        if (!mbr) {
            setErrorPrompt(L"Failed to allocate memory for MBR!");
            showErrorPrompt(L"OK");
            FSADelClient(fsaHandle);
            return;
        }

        WHBLogFreetypeStartScreen();
        WHBLogFreetypePrint(L"Device Info:");
        WHBLogFreetypePrint(getDeviceSummary(fsaHandle, "/dev/sdcard01", deviceInfo).c_str());
        WHBLogFreetypeDraw();

        if (showDialogPrompt(L"Is this the correct device?", L"Yes", L"No", nullptr, nullptr, 1, false) != 0) {
            free(mbr);
            continue;
        }

        uint64_t totalSize = (uint64_t)deviceInfo.deviceSizeInSectors * deviceInfo.deviceSectorSize;
        uint64_t twoGiB = 2ULL * 1024 * 1024 * 1024;

        if (totalSize < twoGiB) {
            showDialogPrompt(L"Device is smaller than 2GiB.\nPartitioning isn't supported.\nThe whole card will be formatted to FAT16.", L"OK");

            if (formatWholeDrive(fsaHandle, "/dev/sdcard01", deviceInfo)) {
                shouldDownloadAroma = true;
                free(mbr);
                break;
            }
            free(mbr);
        } else {
            bool hasMbr = false;
            if ((FSStatus)rawRead(fsaHandle, "/dev/sdcard01", 0, 1, mbr, deviceInfo.deviceSectorSize) == FS_STATUS_OK) {
                if (mbr[510] == 0x55 && mbr[511] == 0xAA) hasMbr = true;
            }

            if (!hasMbr) {
                setErrorPrompt(L"Failed to read MBR!");
                showErrorPrompt(L"OK");
                free(mbr);
                continue;
            }


            uint8_t* backupMbr = (uint8_t*)memalign(0x40, deviceInfo.deviceSectorSize);
            if (backupMbr) {
                if ((FSStatus)rawRead(fsaHandle, "/dev/sdcard01", 1, 1, backupMbr, deviceInfo.deviceSectorSize) == FS_STATUS_OK) {
                    if (backupMbr[510] == 0x55 && backupMbr[511] == 0xAA) {
                        uint8_t restoreChoice = showDialogPrompt(L"Backup MBR found at sector 1. Do you want to restore it?", L"Yes", L"No", nullptr, nullptr, 1);
                        if (restoreChoice == 0) {
                            WHBLogPrint("Restoring MBR from backup...");
                            WHBLogFreetypeDraw();
                            if ((FSStatus)rawWrite(fsaHandle, "/dev/sdcard01", 0, 1, backupMbr, deviceInfo.deviceSectorSize) == FS_STATUS_OK) {
                                // Clear backup
                                memset(backupMbr, 0, deviceInfo.deviceSectorSize);
                                rawWrite(fsaHandle, "/dev/sdcard01", 1, 1, backupMbr, deviceInfo.deviceSectorSize);
                                showDialogPrompt(L"MBR restored successfully!", L"OK");
                                free(mbr);
                                free(backupMbr);
                                FSADelClient(fsaHandle);
                                return;
                            } else {
                                setErrorPrompt(L"Failed to restore MBR!");
                                showErrorPrompt(L"OK");
                            }
                        } else if (restoreChoice == 1) {
                            if (showDialogPrompt(L"Do you want to delete the backup MBR?", L"Yes", L"No", nullptr, nullptr, 1) == 0) {
                                 WHBLogPrint("Clearing backup sector...");
                                 WHBLogFreetypeDraw();
                                 memset(backupMbr, 0, deviceInfo.deviceSectorSize);
                                 rawWrite(fsaHandle, "/dev/sdcard01", 1, 1, backupMbr, deviceInfo.deviceSectorSize);
                            }
                        }
                    }
                }
                free(backupMbr);
            }

            std::wstring summary = getDeviceSummary(fsaHandle, "/dev/sdcard01", deviceInfo);
            std::wstring partitionList = summary + L"\nWhat do you want to do?";

            int partitionCount = 0;
            uint32_t lastOccupiedSector = 1;
            if ((FSStatus)rawRead(fsaHandle, "/dev/sdcard01", 0, 1, mbr, deviceInfo.deviceSectorSize) == FS_STATUS_OK) {
                for (int i = 0; i < 4; i++) {
                    uint8_t type = mbr[446 + i * 16 + 4];
                    if (type != 0) {
                        partitionCount++;
                        uint32_t start = read32LE(&mbr[446 + i * 16 + 8]);
                        uint32_t sectors = read32LE(&mbr[446 + i * 16 + 12]);
                        if (start + sectors > lastOccupiedSector) {
                            lastOccupiedSector = start + sectors;
                        }
                    }
                }
            }

            uint64_t unallocatedSpaceEnd = (uint64_t)(deviceInfo.deviceSizeInSectors - lastOccupiedSector) * deviceInfo.deviceSectorSize;
            bool canCreateWiiUPartition = (unallocatedSpaceEnd > 4ULL * 1024 * 1024 * 1024) && (partitionCount < 4) && (partitionCount > 0);

            std::vector<std::wstring> buttons;
            buttons.push_back(L"Format whole drive to FAT32");
            buttons.push_back(L"Partition drive");
            int optFormatWhole = 0;
            int optPartition = 1;
            int optOnlyFormatP1 = -1;
            int optCreateWiiU = -1;
            int optDeleteMbr = -1;
            int optCancel = -1;

            if (partitionCount > 1) {
                optOnlyFormatP1 = (int)buttons.size();
                buttons.push_back(L"Only format Partition 1");
            }
            if (canCreateWiiUPartition) {
                optCreateWiiU = (int)buttons.size();
                buttons.push_back(L"Create Wii U partition");
            }
            if (hasMbr) {
                optDeleteMbr = (int)buttons.size();
                buttons.push_back(L"Delete MBR");
            }
            optCancel = (int)buttons.size();
            buttons.push_back(L"Cancel");

            uint8_t formatChoice = showDialogPrompt(partitionList.c_str(), buttons, optCancel);

            if (formatChoice == optCancel || formatChoice == 255) {
                free(mbr);
                continue;
            }

            if (formatChoice == optFormatWhole) { // Format whole drive to FAT32
                if (formatWholeDrive(fsaHandle, "/dev/sdcard01", deviceInfo)) {
                    shouldDownloadAroma = true;
                    free(mbr);
                    break;
                }
                free(mbr);
            } else if (formatChoice == optPartition) { // Partition drive
                if (partitionDevice(fsaHandle, "/dev/sdcard01", deviceInfo)) {
                    shouldDownloadAroma = true;
                    free(mbr);
                    break;
                }
                free(mbr);
            } else if (optOnlyFormatP1 != -1 && formatChoice == optOnlyFormatP1) { // Only format Partition 1
                if (showDialogPrompt(L"WARNING: This will format the first partition and DELETE ALL DATA on it.\nOther partitions will be preserved.\nDo you want to continue?", L"Yes", L"No", nullptr, nullptr, 1) != 0) {
                    free(mbr);
                    continue;
                }

                WHBLogPrint("Backing up MBR to sector 1...");
                WHBLogFreetypeDraw();
                if ((FSStatus)rawWrite(fsaHandle, "/dev/sdcard01", 1, 1, mbr, deviceInfo.deviceSectorSize) != FS_STATUS_OK) {
                    setErrorPrompt(L"Failed to backup MBR!");
                    showErrorPrompt(L"OK");
                    free(mbr);
                    continue;
                }

                uint32_t p1_size = read32LE(&mbr[446 + 12]);
                bool partialFormatSuccess = false;
                while (true) {
                    WHBLogPrintf("Formatting Partition 1 (size: %u sectors)...", p1_size);
                    WHBLogFreetypeDraw();
                    setCustomFormatSize(p1_size);
                    int32_t f_status = FSA_Format(fsaHandle, "/dev/sdcard01", "fat", 0, 0, 0);
                    if (f_status != FS_STATUS_OK) {
                        WHBLogPrintf("Format failed (status: %d)!\n", f_status);
                        WHBLogFreetypeDraw();
                        setErrorPrompt(L"Failed to format Partition 1!");
                        if (!showErrorPrompt(L"Cancel", true)) {
                            break;
                        }
                    } else {
                        partialFormatSuccess = true;
                        break;
                    }
                }

                if (partialFormatSuccess) {
                    shouldDownloadAroma = true;

                    // Read the new MBR
                    uint8_t* newMbr = (uint8_t*)memalign(0x40, deviceInfo.deviceSectorSize);
                    if (newMbr) {
                        if ((FSStatus)rawRead(fsaHandle, "/dev/sdcard01", 0, 1, newMbr, deviceInfo.deviceSectorSize) == FS_STATUS_OK) {
                            // Restore other partitions from backup MBR (stored in 'mbr' variable)
                            for (int i = 1; i < 4; i++) {
                                memcpy(&newMbr[446 + i * 16], &mbr[446 + i * 16], 16);
                            }

                            WHBLogPrint("Restoring other partitions to MBR...");
                            WHBLogFreetypeDraw();
                            rawWrite(fsaHandle, "/dev/sdcard01", 0, 1, newMbr, deviceInfo.deviceSectorSize);
                        }
                        free(newMbr);
                    }

                    // Clear backup MBR
                    uint8_t* zeroSector = (uint8_t*)memalign(0x40, deviceInfo.deviceSectorSize);
                    if (zeroSector) {
                        memset(zeroSector, 0, deviceInfo.deviceSectorSize);
                        WHBLogPrint("Clearing backup sector...");
                        WHBLogFreetypeDraw();
                        rawWrite(fsaHandle, "/dev/sdcard01", 1, 1, zeroSector, deviceInfo.deviceSectorSize);
                        free(zeroSector);
                    }
                    free(mbr);
                    break;
                }
                free(mbr);
            } else if (optCreateWiiU != -1 && formatChoice == optCreateWiiU) {
                struct Partition {
                    uint8_t data[16];
                };
                std::vector<Partition> partitions;
                for (int i = 0; i < 4; i++) {
                    if (mbr[446 + i * 16 + 4] != 0) {
                        Partition p;
                        memcpy(p.data, &mbr[446 + i * 16], 16);
                        partitions.push_back(p);
                    }
                }

                memset(&mbr[446], 0, 64);
                for (size_t i = 0; i < partitions.size(); i++) {
                    memcpy(&mbr[446 + i * 16], partitions[i].data, 16);
                }

                uint32_t alignSectors = (64 * 1024 * 1024) / deviceInfo.deviceSectorSize;
                uint32_t p2_start = ((lastOccupiedSector + alignSectors - 1) / alignSectors) * alignSectors;
                if (p2_start < deviceInfo.deviceSizeInSectors) {
                    uint32_t p2_size = deviceInfo.deviceSizeInSectors - p2_start;
                    uint8_t* pte = &mbr[446 + 3 * 16];
                    pte[4] = 0x07; // NTFS/WFS
                    write32LE(&pte[8], p2_start);
                    write32LE(&pte[12], p2_size);

                    // Ensure MBR signature is present
                    mbr[510] = 0x55;
                    mbr[511] = 0xAA;

                    WHBLogPrint("Adding Wii U partition to MBR...");
                    WHBLogFreetypeDraw();
                    if ((FSStatus)rawWrite(fsaHandle, "/dev/sdcard01", 0, 1, mbr, deviceInfo.deviceSectorSize) == FS_STATUS_OK) {
                        writeMbrSignature(fsaHandle, "/dev/sdcard01", p2_start, deviceInfo.deviceSectorSize);
                        showDialogPrompt(L"Wii U partition created successfully!", L"OK");
                    } else {
                        setErrorPrompt(L"Failed to write MBR!");
                        showErrorPrompt(L"OK");
                    }
                }
                free(mbr);
                break;
            } else if (optDeleteMbr != -1 && formatChoice == optDeleteMbr) {
                if (showDialogPrompt(L"WARNING: This will DELETE the MBR and ALL partition information.\nDo you want to continue?", L"Yes", L"No", nullptr, nullptr, 1) == 0) {
                    uint8_t* zeroSector = (uint8_t*)memalign(0x40, deviceInfo.deviceSectorSize);
                    if (zeroSector) {
                        memset(zeroSector, 0, deviceInfo.deviceSectorSize);
                        WHBLogPrint("Deleting MBR...");
                        WHBLogFreetypeDraw();
                        if ((FSStatus)rawWrite(fsaHandle, "/dev/sdcard01", 0, 1, zeroSector, deviceInfo.deviceSectorSize) == FS_STATUS_OK) {
                            showDialogPrompt(L"MBR deleted successfully!", L"OK");
                        } else {
                            setErrorPrompt(L"Failed to delete MBR!");
                            showErrorPrompt(L"OK");
                        }
                        free(zeroSector);
                    }
                }
                free(mbr);
            }
        }
    }

    FSADelClient(fsaHandle);

    if (shouldDownloadAroma) {
        sleep_for(2s);
        if (showDialogPrompt(L"Device formatted successfully!\nDo you want to download Aroma now?", L"Yes", L"No") == 0) {
            downloadAroma("fs:/vol/external01/");
        } else {
            showSuccessPrompt(L"Formatting complete!");
        }
    }
}

void setupSDUSBMenu() {
    WHBLogPrint("Opening /dev/fsa...");
    WHBLogFreetypeDraw();
    FSAClientHandle fsaHandle = FSAAddClient(NULL);
    if (fsaHandle < 0) {
        WHBLogPrintf("Failed to open /dev/fsa! Status: 0x%08X", fsaHandle);
        WHBLogFreetypeDraw();
        setErrorPrompt(L"Failed to open /dev/fsa!");
        showErrorPrompt(L"OK");
        return;
    }

    usbAsSd(false);

    WHBLogFreetypePrint(L"Unmounting SD card...");
    WHBLogFreetypeDrawScreen();
    int status = WHBUnmountSdCard();
    if (status != 1) {
        WHBLogPrintf("Unmount failed (status: %d), ignoring...", status);
        WHBLogFreetypeDraw();
    }

    while (true) {
        while (true) {
            uint8_t choice = showDialogPrompt(L"Remove all SD and USB Storage devices NOW!", L"OK", L"Cancel");
            if (choice != 0) {
                FSADelClient(fsaHandle);
                return;
            }

            FSADeviceInfo devInfo;
            if ((FSStatus)FSAGetDeviceInfo(fsaHandle, "/dev/sdcard01", &devInfo) != FS_STATUS_OK) {
                break;
            }
        }

        while (true) {
            WHBLogFreetypeStartScreen();
            WHBLogFreetypePrint(L"Plug in the SD card you want to use for SDUSB");
            WHBLogFreetypePrint(L"");
            WHBLogFreetypePrint(L"Waiting for device...");
            WHBLogFreetypePrint(L"Press B to cancel");
            WHBLogFreetypeDrawScreen();

            FSADeviceInfo devInfo;
            if ((FSStatus)FSAGetDeviceInfo(fsaHandle, "/dev/sdcard01", &devInfo) == FS_STATUS_OK) {
                break;
            }

            updateInputs();
            if (pressedBack()) {
                FSADelClient(fsaHandle);
                return;
            }
            OSSleepTicks(OSMillisecondsToTicks(100));
        }

        FSADeviceInfo deviceInfo;
        status = (int)FSAGetDeviceInfo(fsaHandle, "/dev/sdcard01", &deviceInfo);
        if ((FSStatus)status != FS_STATUS_OK) {
            WHBLogPrintf("FSAGetDeviceInfo failed: %d", status);
            WHBLogFreetypeDraw();
            setErrorPrompt(L"Failed to get device info!");
            showErrorPrompt(L"OK");
            FSADelClient(fsaHandle);
            return;
        }

        uint8_t* mbr = (uint8_t*)memalign(0x40, deviceInfo.deviceSectorSize);
        if (!mbr) {
            setErrorPrompt(L"Failed to allocate memory for MBR!");
            showErrorPrompt(L"OK");
            FSADelClient(fsaHandle);
            return;
        }

        WHBLogFreetypeStartScreen();
        WHBLogFreetypePrint(L"Device Info:");
        WHBLogFreetypePrint(getDeviceSummary(fsaHandle, "/dev/sdcard01", deviceInfo).c_str());
        WHBLogFreetypeDraw();

        if (showDialogPrompt(L"Is this the correct device?", L"Yes", L"No", nullptr, nullptr, 1, false) != 0) {
            free(mbr);
            continue;
        }

        bool hasNtfs = false;
        int partitionCount = 0;
        uint32_t lastOccupiedSector = 1;
        if ((FSStatus)rawRead(fsaHandle, "/dev/sdcard01", 0, 1, mbr, deviceInfo.deviceSectorSize) == FS_STATUS_OK) {
            if (mbr[510] == 0x55 && mbr[511] == 0xAA) {
                for (int i = 0; i < 4; i++) {
                    uint8_t type = mbr[446 + i * 16 + 4];
                    if (type != 0) {
                        partitionCount++;
                        if (type == 0x07) hasNtfs = true;
                        uint32_t start = read32LE(&mbr[446 + i * 16 + 8]);
                        uint32_t sectors = read32LE(&mbr[446 + i * 16 + 12]);
                        if (start + sectors > lastOccupiedSector) {
                            lastOccupiedSector = start + sectors;
                        }
                    }
                }
            }
        }

        uint64_t unallocatedSpaceEnd = (uint64_t)(deviceInfo.deviceSizeInSectors - lastOccupiedSector) * deviceInfo.deviceSectorSize;
        bool hasSpace = (unallocatedSpaceEnd > 4ULL * 1024 * 1024 * 1024) && (partitionCount < 4) && (partitionCount > 0);

        std::vector<std::wstring> buttons;
        int optKeep = -1;
        int optCreate = -1;
        int optRepartition = -1;
        int optCancel = -1;

        if (hasNtfs) {
            optKeep = (int)buttons.size();
            buttons.push_back(L"Keep current partitioning");
        }
        if (hasSpace) {
            optCreate = (int)buttons.size();
            buttons.push_back(L"Create additional WFS partition");
        }
        optRepartition = (int)buttons.size();
        buttons.push_back(L"Repartition");
        optCancel = (int)buttons.size();
        buttons.push_back(L"Cancel");

        uint8_t choice = showDialogPrompt(L"How do you want to partition the SD card?", buttons, 0);

        bool partitionSuccess = false;
        if (choice == optKeep) {
            partitionSuccess = true;
        } else if (optCreate != -1 && choice == optCreate) {
            struct Partition {
                uint8_t data[16];
            };
            std::vector<Partition> partitions;
            for (int i = 0; i < 4; i++) {
                if (mbr[446 + i * 16 + 4] != 0) {
                    Partition p;
                    memcpy(p.data, &mbr[446 + i * 16], 16);
                    partitions.push_back(p);
                }
            }

            memset(&mbr[446], 0, 64);
            for (size_t i = 0; i < partitions.size(); i++) {
                memcpy(&mbr[446 + i * 16], partitions[i].data, 16);
            }

            uint32_t alignSectors = (64 * 1024 * 1024) / deviceInfo.deviceSectorSize;
            uint32_t p_start = ((lastOccupiedSector + alignSectors - 1) / alignSectors) * alignSectors;
            if (p_start < deviceInfo.deviceSizeInSectors) {
                uint32_t p_size = deviceInfo.deviceSizeInSectors - p_start;
                uint8_t* pte = &mbr[446 + 3 * 16];
                pte[4] = 0x07;
                write32LE(&pte[8], p_start);
                write32LE(&pte[12], p_size);
                mbr[510] = 0x55;
                mbr[511] = 0xAA;

                WHBLogPrint("Adding WFS partition to MBR...");
                WHBLogFreetypeDraw();
                if ((FSStatus)rawWrite(fsaHandle, "/dev/sdcard01", 0, 1, mbr, deviceInfo.deviceSectorSize) == FS_STATUS_OK) {
                    writeMbrSignature(fsaHandle, "/dev/sdcard01", p_start, deviceInfo.deviceSectorSize);
                    showDialogPrompt(L"WFS partition created successfully!", L"OK");
                    partitionSuccess = true;
                } else {
                    setErrorPrompt(L"Failed to write MBR!");
                    showErrorPrompt(L"OK");
                }
            }
        } else if (choice == optRepartition) {
            partitionSuccess = partitionDevice(fsaHandle, "/dev/sdcard01", deviceInfo);
        }

        free(mbr);

        if (partitionSuccess) {
            WHBMountSdCard();
            if (download5sdusb(true, true)) {
                if (!dirExist("fs:/vol/external01/wiiu/environments/aroma")) {
                    if (showDialogPrompt(L"Aroma is missing on your SD card.\nDo you want to download it now?", L"Yes", L"No") == 0) {
                        downloadAroma();
                    }
                }

                if (!isStroopwafelAvailable()) {
                    if (downloadHaxFiles()) {
                        downloadHaxFilesToSD();
                        showDialogPrompt(L"The ISFShax installer is controlled with the buttons on the main console.\nPOWER: moves the curser\nEJECT: confirm\nPress A to launch into the ISFShax Installer", L"Continue");
                        loadFwImg();
                    }
                } else {
                    showSuccessPrompt(L"SDUSB setup complete!");
                }
            }
            break;
        }

        if (choice == optCancel || choice == 255) {
            break;
        }
    }

    FSADelClient(fsaHandle);
}
