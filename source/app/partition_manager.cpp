#include "partition_manager.h"
#include "common_paths.h"
#include "startup_checks.h"
#include "menu.h"
#include "isfshax_menu.h"
#include "gui.h"
#include "filesystem.h"
#include "download.h"
#include "common.h"
#include "navigation.h"
#include "cfw.h"
#include "fw_img_loader.h"
#include <malloc.h>
#include <cmath>
#include <cstring>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <coreinit/ios.h>
#include <coreinit/filesystem_fsa.h>
#include <coreinit/time.h>
#include <coreinit/energysaver.h>
#include <mocha/mocha.h>
#include <thread>
#include <mocha/fsa.h>
#include <whb/sdcard.h>

using namespace std::chrono_literals;

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


#define FAT_MOUNT_CALL 0x1078a948
static uint32_t fatmount_org_ins = 0;
static bool mount_guard_enabled = false;

void setupMountGuard(CFWVersion version) {
    mount_guard_enabled = (version == CFWVersion::MOCHA_FSCLIENT);
}

static void block_fat_mount(void) {
    uint32_t val = 0;
    if (Mocha_IOSUKernelRead32(FAT_MOUNT_CALL, &val) == MOCHA_RESULT_SUCCESS) {
        if (val != 0xe3e00000) {
            fatmount_org_ins = val;
        }
    }
    Mocha_IOSUKernelWrite32(FAT_MOUNT_CALL, 0xe3e00000); // mov r0,#-1
}

static void unblock_fat_mount(void) {
    if (fatmount_org_ins != 0 && fatmount_org_ins != 0xe3e00000) {
        Mocha_IOSUKernelWrite32(FAT_MOUNT_CALL, fatmount_org_ins);
    }
}

FatMountGuard::FatMountGuard() : active(false) {}
FatMountGuard::~FatMountGuard() { unblock(); }
void FatMountGuard::block() {
    if (!active && mount_guard_enabled) {
        // We only need the mountguard when we are running aroma, which is indicated by the presence of Mocha.
        // In the other cases the patch to block mounts causes problems (it causes the mount to hang after releasing the guard).
        block_fat_mount();
        active = true;
    }
}
void FatMountGuard::unblock() {
    if (active) {
        unblock_fat_mount();
        showDialogPrompt(L"The FAT mount block has been released.\nPlease REPLUG your SD card now to ensure it is detected correctly.", L"OK");
        active = false;
    }
}

void FatMountGuard::silent_unblock() {
    if (active) {
        unblock_fat_mount();
        active = false;
    }
}

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
        case 0x17: return "WFS";
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

bool deleteMbr(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo) {
    uint8_t* zeroSector = (uint8_t*)memalign(0x40, deviceInfo.deviceSectorSize);
    if (!zeroSector) {
        setErrorPrompt(L"Failed to allocate memory for zeroing MBR!");
        showErrorPrompt(L"OK");
        return false;
    }

    memset(zeroSector, 0, deviceInfo.deviceSectorSize);
    WHBLogPrint("Deleting MBR...");
    WHBLogFreetypeDraw();
    FSError res = rawWrite(fsaHandle, device, 0, 1, zeroSector, deviceInfo.deviceSectorSize);
    free(zeroSector);

    if ((FSStatus)res == FS_STATUS_OK) {
        showDialogPrompt(L"MBR deleted successfully!", L"OK");
        return true;
    } else {
        setErrorPrompt(L"Failed to delete MBR!");
        showErrorPrompt(L"OK");
        return false;
    }
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

void showDeviceInfoScreen(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo) {
    WHBLogFreetypeStartScreen();
    WHBLogFreetypePrint(L"Device Info:");
    WHBLogFreetypePrint(getDeviceSummary(fsaHandle, device, deviceInfo).c_str());
    WHBLogFreetypePrint(L" ");
    WHBLogFreetypeDraw();
}

bool waitForDevice(FSAClientHandle fsaHandle, const wchar_t* deviceName, FatMountGuard& guard) {
    while (true) {
        CHECK_SHUTDOWN_VAL(false);
        const wchar_t* msg = L"Remove ALL SD and USB storage devices NOW!";
        if (isUsbMounted()) {
            msg = L"To prevent a crash, please SHUTDOWN the console\n"
                  L"and then unplug the USB device or the SD card\n"
                  L"in case you are using SDUSB.\n"
                  L"Relaunch the installer from wafel.xyz\n"
                  L"if Aroma doesn't load.";
            uint8_t choice = showDialogPrompt(msg, L"Shutdown", L"Cancel");
            if (choice == 0) {
                setShutdownPending(true, true);
            }
            return false;
        }
        uint8_t choice = showDialogPrompt(L"Remove ALL SD and USB storage devices NOW!", L"OK", L"Cancel");
        if (choice != 0) return false;

        FSADeviceInfo devInfo;
        if ((FSStatus)FSAGetDeviceInfo(fsaHandle, "/dev/sdcard01", &devInfo) != FS_STATUS_OK) {
            break;
        }
    }

    guard.block();

    // stroopwafel might be gone now, better do a full reboot
    setFullRebootPending(true);

    std::wstring pluginMsg = L"Plug in ONLY the " + std::wstring(deviceName) + L" you want to work with.\nPlugging in other devices may lead to DATA LOSS!";
    while (true) {
        CHECK_SHUTDOWN_VAL(false);
        WHBLogFreetypeStartScreen();
        WHBLogFreetypePrint(pluginMsg.c_str());
        WHBLogFreetypePrint(L" ");
        WHBLogFreetypePrint(L"Waiting for device...");
        WHBLogFreetypePrint(L"Press B to cancel");
        WHBLogFreetypeDrawScreen();

        FSADeviceInfo devInfo;
        if ((FSStatus)FSAGetDeviceInfo(fsaHandle, "/dev/sdcard01", &devInfo) == FS_STATUS_OK) {
            return true;
        }

        updateInputs();
        if (pressedBack()) return false;
        OSSleepTicks(OSMillisecondsToTicks(100));
    }
}

bool formatWholeDrive(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo) {
    FatMountGuard guard;
    guard.block();

    uint64_t totalSize = (uint64_t)deviceInfo.deviceSizeInSectors * deviceInfo.deviceSectorSize;
    uint64_t twoGiB = 2ULL * 1024 * 1024 * 1024;
    const char* fsType = (totalSize < twoGiB) ? "fat" : "fat"; // FAT16 is used by system automatically if < 2GiB and we pass "fat"

    showDeviceInfoScreen(fsaHandle, device, deviceInfo);
    if (showDialogPrompt(L"WARNING: This will format the whole device and DELETE ALL DATA on it.\nDo you want to continue?", L"Yes", L"No", nullptr, nullptr, 1, false) != 0) {
        return false;
    }

    while (true) {
        setCustomFormatSize(0);
        WHBLogFreetypePrint(L"Formatting whole Device with FAT32...\nthis might take a while");
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
    FatMountGuard guard;
    guard.block();

    uint8_t* mbr_check = (uint8_t*)memalign(0x40, deviceInfo.deviceSectorSize);
    if (mbr_check) {
        if ((FSStatus)rawRead(fsaHandle, device, 0, 1, mbr_check, deviceInfo.deviceSectorSize) == FS_STATUS_OK) {
            if (mbr_check[510] == 0x55 && mbr_check[511] == 0xAA) {
                bool hasFat = false;
                for (int i = 0; i < 4; i++) {
                    uint8_t type = mbr_check[446 + i * 16 + 4];
                    if (type == 0x0B || type == 0x0C) {
                        hasFat = true;
                        break;
                    }
                }
                if (hasFat) {
                    showDialogPrompt(L"Note: If you want to keep the data on your FAT32 partition, you should use a\nPC to resize it. Doing it on the Wii U will DELETE ALL DATA on the device.", L"OK");
                }
            }
        }
        free(mbr_check);
    }

    double totalGB = (double)deviceInfo.deviceSizeInSectors * (double)deviceInfo.deviceSectorSize / (1024.0 * 1024.0 * 1024.0);
    double targetFatGB = (totalGB >= 40.0) ? 20.0 : (totalGB / 2.0);
    int fatPercent = (int)std::round((targetFatGB / totalGB) * 100.0);
    if (fatPercent < 0) fatPercent = 0;
    if (fatPercent > 100) fatPercent = 100;

    uint32_t alignSectors = (64 * 1024 * 1024) / deviceInfo.deviceSectorSize;
    uint32_t final_p2_start = 0;

    while(true) {
        uint32_t target_p1_size = (uint32_t)(deviceInfo.deviceSizeInSectors * fatPercent / 100.0);
        uint32_t p2_start = ((target_p1_size + alignSectors - 1) / alignSectors) * alignSectors;
        if (p2_start > deviceInfo.deviceSizeInSectors) p2_start = deviceInfo.deviceSizeInSectors;

        final_p2_start = p2_start;

        double fatGB = (double)final_p2_start * deviceInfo.deviceSectorSize / (1024.0 * 1024.0 * 1024.0);
        double ntfsGB = (double)(deviceInfo.deviceSizeInSectors - final_p2_start) * deviceInfo.deviceSectorSize / (1024.0 * 1024.0 * 1024.0);

        showDeviceInfoScreen(fsaHandle, device, deviceInfo);
        WHBLogFreetypePrint((L"Partition " + toWstring(device)).c_str());
        WHBLogFreetypePrint(L"===============================");
        WHBLogFreetypePrintf(L"FAT32:  [%3d%%] (%6.2f GB)    Homebrew and vWii USB Loader", fatPercent, fatGB);
        WHBLogFreetypePrintf(L"WFS:      [%3d%%] (%6.2f GB)    Wii U games and VC", 100 - fatPercent, ntfsGB);
        WHBLogFreetypePrint(L" ");
        WHBLogFreetypePrint(L"Use Left/Right to adjust (1% increments)");
        WHBLogFreetypePrint(L"Use Up/Down to adjust (10% increments)");
        WHBLogFreetypePrint(L"Press A to confirm, B to cancel");
        WHBLogFreetypeScreenPrintBottom(L"===============================");
        WHBLogFreetypeDrawScreen();

        OSSleepTicks(OSMillisecondsToTicks(100));
        updateInputs();
        bool confirmed = false;
        bool cancelled = false;
        while(true) {
            updateInputs();
            if (navigatedLeft() && fatPercent > 0) {
                if (totalGB >= 1.0) {
                    if (totalGB * (fatPercent - 1) / 100.0 >= 1.0) {
                        fatPercent -= 1;
                    }
                } else {
                    fatPercent -= 1;
                }
                break;
            }
            if (navigatedRight() && fatPercent < 100) {
                fatPercent += 1;
                break;
            }
            if (navigatedDown() && fatPercent > 0) {
                if (totalGB >= 1.0) {
                    if (totalGB * (fatPercent - 10) / 100.0 >= 1.0) {
                        fatPercent -= 10;
                    } else {
                        fatPercent = (int)std::ceil(100.0 / totalGB);
                    }
                } else {
                    fatPercent = (fatPercent >= 10) ? fatPercent - 10 : 0;
                }
                break;
            }
            if (navigatedUp() && fatPercent < 100) {
                fatPercent = (fatPercent <= 90) ? fatPercent + 10 : 100;
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

    showDeviceInfoScreen(fsaHandle, device, deviceInfo);
    if (showDialogPrompt(L"WARNING: This will RE-PARTITION the whole device\nand DELETE ALL DATA on it.\nDo you want to continue?", L"Yes", L"No", nullptr, nullptr, 1, false) != 0) {
        return false;
    }

    while (true) {
        WHBLogPrint("Formatting FAT32 partition...\nthis might take a while");
        WHBLogFreetypeDraw();
        // that also incldues the MBR and alignement gap
        setCustomFormatSize(final_p2_start);
        WHBUnmountSdCard();
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

        if(actual_p1_start + actual_p1_size > final_p2_start){
            setErrorPrompt(L"Failed to partition, FAT32 partition too large!");
            if (!showErrorPrompt(L"Cancel", true)) {
                return false;
            }
        }

        if (final_p2_start < deviceInfo.deviceSizeInSectors) {
            uint32_t p2_size = (uint32_t)(deviceInfo.deviceSizeInSectors - final_p2_start);

            uint8_t* pte2 = &mbr[446 + 3 * 16];
            pte2[4] = 0x17;
            write32LE(&pte2[8], final_p2_start);
            write32LE(&pte2[12], p2_size);

            WHBLogPrint("Adding second partition to MBR...");
            WHBLogFreetypeDraw();
            if ((FSStatus)rawWrite(fsaHandle, device, 0, 1, mbr, deviceInfo.deviceSectorSize) == FS_STATUS_OK) {
                writeMbrSignature(fsaHandle, device, final_p2_start, deviceInfo.deviceSectorSize);
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

bool fixPartitionOrder(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo) {
    uint8_t* mbr = (uint8_t*)memalign(0x40, deviceInfo.deviceSectorSize);
    if (!mbr) return false;

    if ((FSStatus)rawRead(fsaHandle, device, 0, 1, mbr, deviceInfo.deviceSectorSize) != FS_STATUS_OK) {
        free(mbr);
        return false;
    }

    if (mbr[510] != 0x55 || mbr[511] != 0xAA) {
        free(mbr);
        return false;
    }

    int fatIndex = -1;
    for (int i = 0; i < 4; i++) {
        uint8_t type = mbr[446 + i * 16 + 4];
        if (type == 0x0B || type == 0x0C) {
            fatIndex = i;
            break;
        }
    }

    if (fatIndex <= 0) {
        free(mbr);
        return false;
    }

    struct PartitionEntry {
        uint8_t data[16];
    };
    std::vector<PartitionEntry> entries;
    PartitionEntry fatEntry;
    memcpy(fatEntry.data, &mbr[446 + fatIndex * 16], 16);
    entries.push_back(fatEntry);

    for (int i = 0; i < 4; i++) {
        if (i == fatIndex) continue;
        uint8_t type = mbr[446 + i * 16 + 4];
        if (type != 0) {
            PartitionEntry e;
            memcpy(e.data, &mbr[446 + i * 16], 16);
            entries.push_back(e);
        }
    }

    memset(&mbr[446], 0, 64);
    for (size_t i = 0; i < entries.size() && i < 4; i++) {
        memcpy(&mbr[446 + i * 16], entries[i].data, 16);
    }

    WHBLogPrint("Fixing partition order in MBR...");
    WHBLogFreetypeDraw();
    bool success = (FSStatus)rawWrite(fsaHandle, device, 0, 1, mbr, deviceInfo.deviceSectorSize) == FS_STATUS_OK;
    free(mbr);
    return success;
}

bool checkAndFixPartitionOrder(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo, bool& repartitioned) {
    repartitioned = false;
    uint8_t* mbr = (uint8_t*)memalign(0x40, deviceInfo.deviceSectorSize);
    if (!mbr) return false;

    int fatIndex = -1;
    if ((FSStatus)rawRead(fsaHandle, device, 0, 1, mbr, deviceInfo.deviceSectorSize) == FS_STATUS_OK) {
        if (mbr[510] == 0x55 && mbr[511] == 0xAA) {
            for (int i = 0; i < 4; i++) {
                uint8_t type = mbr[446 + i * 16 + 4];
                if (type == 0x0B || type == 0x0C) {
                    fatIndex = i;
                    break;
                }
            }
        }
    }
    free(mbr);

    if (fatIndex <= 0) return true; // OK or not found

    showDeviceInfoScreen(fsaHandle, device, deviceInfo);
    uint8_t fixChoice = showDialogPrompt(L"FAT32 partition found but it is not the first partition.\nThis may cause issues with some homebrew.\nDo you want to fix the partition order or repartition?", L"Fix order", L"Repartition", L"Cancel", nullptr, 0, false);
    if (fixChoice == 0) {
        FatMountGuard guard;
        guard.block();
        if (fixPartitionOrder(fsaHandle, device, deviceInfo)) {
            showDialogPrompt(L"Partition order fixed successfully!", L"OK");
            return true;
        } else {
            setErrorPrompt(L"Failed to fix partition order!");
            showErrorPrompt(L"OK");
            return false;
        }
    } else if (fixChoice == 1) {
        FatMountGuard guard;
        guard.block();
        repartitioned = partitionDevice(fsaHandle, device, deviceInfo);
        return false;
    }

    return false;
}

bool getMbrPartitionInfo(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo, uint8_t* mbr, MbrPartitionInfo& info) {
    info = MbrPartitionInfo();
    if ((FSStatus)rawRead(fsaHandle, device, 0, 1, mbr, deviceInfo.deviceSectorSize) != FS_STATUS_OK) {
        return false;
    }

    if (mbr[510] == 0x55 && mbr[511] == 0xAA) {
        for (int i = 0; i < 4; i++) {
            uint8_t type = mbr[446 + i * 16 + 4];
            if (type != 0) {
                info.partitionCount++;
                if (i == 0 && (type == 0x0B || type == 0x0C)) info.hasFat32 = true;
                if (type == 0x07 || type == 0x17) info.hasWfs = true;
                uint32_t start = read32LE(&mbr[446 + i * 16 + 8]);
                uint32_t sectors = read32LE(&mbr[446 + i * 16 + 12]);
                if (start + sectors > info.lastOccupiedSector) {
                    info.lastOccupiedSector = start + sectors;
                }
            }
        }
    }
    uint64_t unallocatedSpace = (uint64_t)(deviceInfo.deviceSizeInSectors - info.lastOccupiedSector) * deviceInfo.deviceSectorSize;
    info.hasSpace = (unallocatedSpace > 4ULL * 1024 * 1024 * 1024) && (info.partitionCount < 4) && (info.partitionCount > 0);
    return true;
}

static bool addWiiUPartition(FSAClientHandle fsaHandle, const FSADeviceInfo& deviceInfo, uint8_t* mbr, uint32_t lastOccupiedSector) {
    struct PartitionEntryHelper {
        uint8_t data[16];
    };
    std::vector<PartitionEntryHelper> partitions;
    for (int i = 0; i < 4; i++) {
        if (mbr[446 + i * 16 + 4] != 0) {
            PartitionEntryHelper p;
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
        pte[4] = 0x17;
        write32LE(&pte[8], p_start);
        write32LE(&pte[12], p_size);
        mbr[510] = 0x55;
        mbr[511] = 0xAA;

        WHBLogPrint("Adding Wii U partition to MBR...");
        WHBLogFreetypeDraw();
        if ((FSStatus)rawWrite(fsaHandle, "/dev/sdcard01", 0, 1, mbr, deviceInfo.deviceSectorSize) == FS_STATUS_OK) {
            writeMbrSignature(fsaHandle, "/dev/sdcard01", p_start, deviceInfo.deviceSectorSize);
            showDialogPrompt(L"Wii U partition created successfully!", L"OK");
            return true;
        } else {
            setErrorPrompt(L"Failed to write MBR!");
            showErrorPrompt(L"OK");
        }
    }
    return false;
}

bool handlePartitionActionMenu(FSAClientHandle fsaHandle, const FSADeviceInfo& deviceInfo, const wchar_t* deviceTypeName, bool needWFS) {
    uint8_t* mbr = (uint8_t*)memalign(0x40, deviceInfo.deviceSectorSize);
    if (!mbr) {
        setErrorPrompt(L"Failed to allocate memory for MBR!");
        showErrorPrompt(L"OK");
        return false;
    }

    bool partitionSuccess = false;
    while (!partitionSuccess) {
        MbrPartitionInfo info;
        if (!getMbrPartitionInfo(fsaHandle, "/dev/sdcard01", deviceInfo, mbr, info)) {
            setErrorPrompt(L"Failed to read MBR from device!");
            showErrorPrompt(L"OK");
            break;
        }

        std::vector<std::wstring> buttons;
        int optKeep = -1, optCreate = -1, optRepartition = -1, optCancel = -1;

        if (info.hasFat32 && (!needWFS || info.hasWfs)) {
            optKeep = (int)buttons.size();
            buttons.push_back(L"Keep current partitioning");
        }
        if (info.hasSpace && !info.hasWfs) {
            optCreate = (int)buttons.size();
            buttons.push_back(L"Create additional Wii U partition");
        }
        optRepartition = (int)buttons.size();
        buttons.push_back(L"Repartition");
        optCancel = (int)buttons.size();
        buttons.push_back(L"Cancel");

        showDeviceInfoScreen(fsaHandle, "/dev/sdcard01", deviceInfo);
        std::wstring prompt = L"How do you want to partition the " + std::wstring(deviceTypeName) + L"?";
        uint8_t choice = showDialogPrompt(prompt.c_str(), buttons, 0, false);

        if (choice == optKeep) {
            partitionSuccess = true;
        } else if (optCreate != -1 && choice == optCreate) {
            if (addWiiUPartition(fsaHandle, deviceInfo, mbr, info.lastOccupiedSector)) {
                partitionSuccess = true;
            }
        } else if (choice == optRepartition) {
            partitionSuccess = partitionDevice(fsaHandle, "/dev/sdcard01", deviceInfo);
        } else if (choice == optCancel || choice == 255) {
            break;
        }
    }

    free(mbr);
    return partitionSuccess;
}

bool checkSdCardPartitioning(FSAClientHandle fsaHandle, const FSADeviceInfo& deviceInfo) {
    uint8_t* mbr = (uint8_t*)memalign(0x40, deviceInfo.deviceSectorSize);
    if (!mbr) return false;

    bool wantsPartitionedStorage = false;

    // This function is now only called if hasWiiuDir is false
    bool stage1Done = false;
    while (!stage1Done) {
        uint8_t choice = showDialogPrompt(L"The SD card seems to be new (no 'wiiu' folder found).\nDo you also want to use the SD card to store Wii U games?", L"Yes", L"No", L"Cancel");
        if (choice == 1) { // No
            MbrPartitionInfo info;
            if (!getMbrPartitionInfo(fsaHandle, "/dev/sdcard01", deviceInfo, mbr, info)) {
                setErrorPrompt(L"Failed to read MBR from device!");
                showErrorPrompt(L"OK");
                break;
            }

            uint32_t p1_end = 0;
            if (info.hasFat32) {
                uint32_t p1_start = read32LE(&mbr[446 + 8]);
                uint32_t p1_size = read32LE(&mbr[446 + 12]);
                p1_end = p1_start + p1_size;
            }

            uint64_t remainingSpace = (uint64_t)(deviceInfo.deviceSizeInSectors - p1_end) * deviceInfo.deviceSectorSize;
            bool isPerfectFat32 = (info.partitionCount == 1) && info.hasFat32 && (remainingSpace < 128ULL * 1024 * 1024);

            if (!isPerfectFat32) {
                std::wstring msg = L"The SD card is not formatted to use the full space for homebrew";
                if (info.partitionCount > 1) {
                    msg += L" (it has multiple partitions).";
                } else {
                    msg += L".";
                }
                msg += L"\nDo you want to reformat it to use the entire card or keep it as is?";

                uint8_t fixChoice = showDialogPrompt(msg.c_str(), L"Format", L"Keep", L"Cancel");
                if (fixChoice == 0) {
                    if (formatWholeDrive(fsaHandle, "/dev/sdcard01", deviceInfo)) {
                        WHBMountSdCard();
                        stage1Done = true;
                    }
                } else if (fixChoice == 1) {
                    stage1Done = true;
                }
            } else {
                stage1Done = true;
            }
        } else if (choice == 0) { // Yes
            bool stage2Done = false;
            while (!stage2Done) {
                MbrPartitionInfo info;
                if (!getMbrPartitionInfo(fsaHandle, "/dev/sdcard01", deviceInfo, mbr, info)) {
                    setErrorPrompt(L"Failed to read MBR from device!");
                    showErrorPrompt(L"OK");
                    break;
                }

                std::vector<std::wstring> buttons;
                int optKeep = -1, optPartition = -1, optCreateWiiU = -1;

                if (info.hasFat32 && info.hasWfs) {
                    optKeep = (int)buttons.size();
                    buttons.push_back(L"Keep current partitioning");
                }
                optPartition = (int)buttons.size();
                buttons.push_back(L"Partition for homebrew and Wii U games");
                if (info.hasSpace && !info.hasWfs) {
                    optCreateWiiU = (int)buttons.size();
                    buttons.push_back(L"Add Wii U partition");
                }
                buttons.push_back(L"Back");
                int optBack = (int)buttons.size() - 1;

                showDeviceInfoScreen(fsaHandle, "/dev/sdcard01", deviceInfo);
                uint8_t s2Choice = showDialogPrompt(L"How do you want to partition the SD card?", buttons, 0, false);

                if (s2Choice == optBack || s2Choice == 255) {
                    stage2Done = true;
                } else {
                    if (s2Choice == optKeep) {
                        wantsPartitionedStorage = true;
                    } else if (s2Choice == optPartition) {
                        if (partitionDevice(fsaHandle, "/dev/sdcard01", deviceInfo)) {
                            wantsPartitionedStorage = true;
                            WHBMountSdCard();
                        }
                    } else if (optCreateWiiU != -1 && s2Choice == optCreateWiiU) {
                        if (addWiiUPartition(fsaHandle, deviceInfo, mbr, info.lastOccupiedSector)) {
                            wantsPartitionedStorage = true;
                        }
                    }
                    stage2Done = true;
                    stage1Done = true;
                }
            }
        } else {
            stage1Done = true;
        }
    }

    free(mbr);
    return wantsPartitionedStorage;
}

bool handleSDUSBAction(FSAClientHandle fsaHandle, const FSADeviceInfo& deviceInfo, FatMountGuard& guard) {
    if (handlePartitionActionMenu(fsaHandle, deviceInfo, L"SD card", true)) {
        guard.unblock();
        WHBMountSdCard();
        return true;
    }
    return false;
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
    FatMountGuard guard;

    while (true) {
        if (!waitForDevice(fsaHandle, deviceName, guard)) {
            FSADelClient(fsaHandle);
            usbAsSd(false);
            return;
        }

        FSADeviceInfo deviceInfo;
        status = (int)FSAGetDeviceInfo(fsaHandle, "/dev/sdcard01", &deviceInfo);
        if ((FSStatus)status != FS_STATUS_OK) {
            WHBLogPrintf("FSAGetDeviceInfo failed: %d", status);
            WHBLogFreetypeDraw();
            setErrorPrompt(L"Failed to get device info!");
            showErrorPrompt(L"OK");
            FSADelClient(fsaHandle);
            usbAsSd(false);
            return;
        }

        uint8_t* mbr = (uint8_t*)memalign(0x40, deviceInfo.deviceSectorSize);
        if (!mbr) {
            setErrorPrompt(L"Failed to allocate memory for MBR!");
            showErrorPrompt(L"OK");
            FSADelClient(fsaHandle);
            usbAsSd(false);
            return;
        }

        showDeviceInfoScreen(fsaHandle, "/dev/sdcard01", deviceInfo);

        if (showDialogPrompt(L"Is this the correct device?", L"Yes", L"No", nullptr, nullptr, 1, false) != 0) {
            free(mbr);
            continue;
        }

        uint64_t totalSize = (uint64_t)deviceInfo.deviceSizeInSectors * deviceInfo.deviceSectorSize;
        uint64_t twoGiB = 2ULL * 1024 * 1024 * 1024;

        bool actionCompleted = false;
        while (!actionCompleted) {
            if (totalSize < twoGiB) {
                showDeviceInfoScreen(fsaHandle, "/dev/sdcard01", deviceInfo);
                showDialogPrompt(L"Device is smaller than 2GiB.\nPartitioning isn't supported.\nThe whole card will be formatted to FAT16.", L"OK", false);

                if (formatWholeDrive(fsaHandle, "/dev/sdcard01", deviceInfo)) {
                    shouldDownloadAroma = true;
                    actionCompleted = true;
                }
                break;
            } else {
                uint8_t* backupMbr = (uint8_t*)memalign(0x40, deviceInfo.deviceSectorSize);
                if (backupMbr) {
                    if ((FSStatus)rawRead(fsaHandle, "/dev/sdcard01", 1, 1, backupMbr, deviceInfo.deviceSectorSize) == FS_STATUS_OK) {
                        if (backupMbr[510] == 0x55 && backupMbr[511] == 0xAA) {
                            showDeviceInfoScreen(fsaHandle, "/dev/sdcard01", deviceInfo);
                            uint8_t restoreChoice = showDialogPrompt(L"Backup MBR found at sector 1. Do you want to restore it?", L"Yes", L"No", nullptr, nullptr, 1, false);
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
                                showDeviceInfoScreen(fsaHandle, "/dev/sdcard01", deviceInfo);
                                if (showDialogPrompt(L"Do you want to delete the backup MBR?", L"Yes", L"No", nullptr, nullptr, 1, false) == 0) {
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

                MbrPartitionInfo info;
                if (!getMbrPartitionInfo(fsaHandle, "/dev/sdcard01", deviceInfo, mbr, info)) {
                    setErrorPrompt(L"Failed to read MBR from device!");
                    showErrorPrompt(L"OK");
                    break;
                }

                std::vector<std::wstring> buttons;
                buttons.push_back(L"Format whole drive to FAT32");
                buttons.push_back(L"Partition drive");
                int optFormatWhole = 0;
                int optPartition = 1;
                int optOnlyFormatP1 = -1;
                int optCreateWiiU = -1;
                int optDeleteMbr = -1;
                int optCancel = -1;

                if (info.partitionCount > 1) {
                    optOnlyFormatP1 = (int)buttons.size();
                    buttons.push_back(L"Only format Partition 1");
                }
                if (info.hasSpace && !info.hasWfs) {
                    optCreateWiiU = (int)buttons.size();
                    buttons.push_back(L"Create Wii U partition");
                }
                if (mbr[510] == 0x55 && mbr[511] == 0xAA) {
                    optDeleteMbr = (int)buttons.size();
                    buttons.push_back(L"Delete MBR");
                }
                optCancel = (int)buttons.size();
                buttons.push_back(L"Cancel");

                showDeviceInfoScreen(fsaHandle, "/dev/sdcard01", deviceInfo);
                uint8_t formatChoice = showDialogPrompt(L"What do you want to do?", buttons, optCancel, false);

                if (formatChoice == optCancel || formatChoice == 255) {
                    break;
                }

                if (formatChoice == optFormatWhole) { // Format whole drive to FAT32
                    if (formatWholeDrive(fsaHandle, "/dev/sdcard01", deviceInfo)) {
                        shouldDownloadAroma = true;
                        actionCompleted = true;
                        break;
                    }
                } else if (formatChoice == optPartition) { // Partition drive
                    if (partitionDevice(fsaHandle, "/dev/sdcard01", deviceInfo)) {
                        shouldDownloadAroma = true;
                        actionCompleted = true;
                        break;
                    }
                } else if (optOnlyFormatP1 != -1 && formatChoice == optOnlyFormatP1) { // Only format Partition 1
                    showDeviceInfoScreen(fsaHandle, "/dev/sdcard01", deviceInfo);
                    if (showDialogPrompt(L"WARNING: This will format the first partition and DELETE ALL DATA on it.\nOther partitions will be preserved.\nDo you want to continue?", L"Yes", L"No", nullptr, nullptr, 1, false) != 0) {
                        continue;
                    }

                    WHBLogPrint("Backing up MBR to sector 1...");
                    WHBLogFreetypeDraw();
                    if ((FSStatus)rawWrite(fsaHandle, "/dev/sdcard01", 1, 1, mbr, deviceInfo.deviceSectorSize) != FS_STATUS_OK) {
                        setErrorPrompt(L"Failed to backup MBR!");
                        showErrorPrompt(L"OK");
                        continue;
                    }

                    uint32_t p1_size = read32LE(&mbr[446 + 12]);
                    bool partialFormatSuccess = false;
                    while (true) {
                        WHBLogPrintf("Formatting Partition 1 (size: %u sectors)...\nthis might take a while", p1_size);
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
                        actionCompleted = true;
                        break;
                    }
                } else if (optCreateWiiU != -1 && formatChoice == optCreateWiiU) {
                    if (addWiiUPartition(fsaHandle, deviceInfo, mbr, info.lastOccupiedSector)) {
                        actionCompleted = true;
                        break;
                    }
                } else if (optDeleteMbr != -1 && formatChoice == optDeleteMbr) {
                    showDeviceInfoScreen(fsaHandle, "/dev/sdcard01", deviceInfo);
                    if (showDialogPrompt(L"WARNING: This will DELETE the MBR and ALL partition information.\nDo you want to continue?", L"Yes", L"No", nullptr, nullptr, 1, false) == 0) {
                        if (deleteMbr(fsaHandle, "/dev/sdcard01", deviceInfo)) {
                            actionCompleted = true;
                            break;
                        }
                    }
                }
            }
        }

        free(mbr);
        if (actionCompleted) break;
    }

    FSADelClient(fsaHandle);
    guard.unblock();

    if (shouldDownloadAroma) {
        sleep_for(2s);
        askAndDownloadAroma();
    }

    if (use_usb) {
        setShutdownPending(true);
        if (showDialogPrompt(L"Operation successful!\nIt is recommended to shutdown the console now\nand plug your SD card back in.\nDo you want to shutdown now?", L"Yes", L"No") == 0) {
            setShutdownPending(true, true);
        }
    } else if (shouldDownloadAroma) {
        showSuccessPrompt(L"Formatting complete!");
    }

    usbAsSd(false);
}

bool uninstallSDUSB() {
    WHBMountSdCard();
    bool removed = false;
    std::string sdPlugin = Paths::SdPluginsDir + "/5sdusb.ipx";
    std::string slcPlugin = Paths::SlcPluginsDir + "/5sdusb.ipx";

    if (fileExist(sdPlugin)) {
        if (removeFile(sdPlugin)) {
            removed = true;
        }
    }
    if (fileExist(slcPlugin)) {
        if (removeFile(slcPlugin)) {
            removed = true;
        }
    }

    if (removed) {
        showSuccessPrompt(L"SDUSB plugin removed successfully.");
    } else {
        showDialogPrompt(L"SDUSB plugin not found.", L"OK");
    }

    if (showDialogPrompt(L"Do you want to unpartition the SD card now?\nThis will DELETE ALL DATA on it.", L"Yes, format SD", L"No") == 0) {
        FSAClientHandle fsaHandle = FSAAddClient(NULL);
        if (fsaHandle >= 0) {
            usbAsSd(false);
            WHBUnmountSdCard();
            FatMountGuard guard;
            if (waitForDevice(fsaHandle, L"SD card", guard)) {
                FSADeviceInfo deviceInfo;
                if ((FSStatus)FSAGetDeviceInfo(fsaHandle, "/dev/sdcard01", &deviceInfo) == FS_STATUS_OK) {
                    if (formatWholeDrive(fsaHandle, "/dev/sdcard01", deviceInfo)) {
                        guard.unblock();
                        WHBMountSdCard();

                        // Check if stroopwafel was on SD
                        std::string stroopPath = getStroopwafelPluginPath();
                        bool stroopOnSd = (stroopPath.find(Paths::SdRoot) != std::string::npos) || fileExist(Paths::SdFwImg);

                        if (stroopOnSd) {
                            if (showDialogPrompt(L"Stroopwafel was installed on the SD card.\nDo you want to redownload it now?", L"Yes", L"No") == 0) {
                                if (downloadStroopwafelFiles(true)) {
                                    showSuccessPrompt(L"Stroopwafel files downloaded successfully!");
                                } else {
                                    showErrorPrompt(L"OK");
                                }
                            }
                        }

                        askAndDownloadAroma();

                        showSuccessPrompt(L"SD card unpartitioned successfully.");
                    }
                }
            }
            FSADelClient(fsaHandle);
        }
    }

    return true;
}

bool uninstallUSBPartition() {
    bool removed = false;
    std::string sdPlugins[] = { Paths::SdPluginsDir + "/5usbpart.ipx", Paths::SdPluginsDir + "/5upartsd.ipx" };
    std::string slcPlugins[] = { Paths::SlcPluginsDir + "/5usbpart.ipx", Paths::SlcPluginsDir + "/5upartsd.ipx" };

    for (const auto& p : sdPlugins) {
        if (fileExist(p)) {
            if (removeFile(p)) removed = true;
        }
    }
    for (const auto& p : slcPlugins) {
        if (fileExist(p)) {
            if (removeFile(p)) removed = true;
        }
    }

    if (removed) {
        showSuccessPrompt(L"USB Partition plugin removed successfully.");
    } else {
        showDialogPrompt(L"USB Partition plugin not found.", L"OK");
    }

    if (showDialogPrompt(L"Do you want to unpartition the USB drive now?\nThis will DELETE ALL DATA on it.", L"Yes, zero MBR", L"No") == 0) {
        FSAClientHandle fsaHandle = FSAAddClient(NULL);
        if (fsaHandle >= 0) {
            usbAsSd(true);
            WHBUnmountSdCard();
            FatMountGuard guard;
            if (waitForDevice(fsaHandle, L"USB device", guard)) {
                FSADeviceInfo deviceInfo;
                if ((FSStatus)FSAGetDeviceInfo(fsaHandle, "/dev/sdcard01", &deviceInfo) == FS_STATUS_OK) {
                    deleteMbr(fsaHandle, "/dev/sdcard01", deviceInfo);
                }
            }
            FSADelClient(fsaHandle);
            usbAsSd(false);
        }
    }

    return true;
}

bool getInstalledUSBPartitionPlugin(std::string& pluginPath, bool& hasEmulation) {
    std::string path = getStroopwafelPluginPath();
    if (!path.empty()) {
        if (fileExist(path + "/5usbpart.ipx")) {
            pluginPath = path;
            hasEmulation = false;
            return true;
        } else if (fileExist(path + "/5upartsd.ipx")) {
            pluginPath = path;
            hasEmulation = true;
            return true;
        }
    }
    return false;
}

void showSDUSBMenu() {
    uint8_t selectedOption = 0;
    while (true) {
        CHECK_SHUTDOWN();
        WHBLogFreetypeStartScreen();
        WHBLogFreetypePrint(L"SDUSB Menu");
        WHBLogFreetypePrint(L"===============================");
        WHBLogFreetypePrintf(L"%C Set up SDUSB", (selectedOption == 0 ? L'>' : L' '));
        WHBLogFreetypePrintf(L"%C Uninstall SDUSB", (selectedOption == 1 ? L'>' : L' '));
        WHBLogFreetypePrintf(L"%C Unpartition SD card (format whole card)", (selectedOption == 2 ? L'>' : L' '));
        WHBLogFreetypePrint(L" ");
        WHBLogFreetypeScreenPrintBottom(L"===============================");
        WHBLogFreetypeScreenPrintBottom(L"\uE000 Button = Select Option \uE001 Button = Back");
        WHBLogFreetypeDrawScreen();

        updateInputs();
        while (true) {
            updateInputs();
            if (navigatedUp() && selectedOption > 0) {
                selectedOption--;
                break;
            }
            if (navigatedDown() && selectedOption < 2) {
                selectedOption++;
                break;
            }
            if (pressedOk()) {
                if (selectedOption == 0) {
                    setupSDUSBMenu();
                } else if (selectedOption == 1) {
                    uninstallSDUSB();
                } else if (selectedOption == 2) {
                    // Unpartition SD Card
                    FSAClientHandle fsaHandle = FSAAddClient(NULL);
                    if (fsaHandle >= 0) {
                        usbAsSd(false);
                        WHBUnmountSdCard();
                        FatMountGuard guard;
                        if (waitForDevice(fsaHandle, L"SD card", guard)) {
                            FSADeviceInfo deviceInfo;
                            if ((FSStatus)FSAGetDeviceInfo(fsaHandle, "/dev/sdcard01", &deviceInfo) == FS_STATUS_OK) {
                                if (formatWholeDrive(fsaHandle, "/dev/sdcard01", deviceInfo)) {
                                    showSuccessPrompt(L"SD card formatted successfully.");
                                    guard.unblock();
                                    WHBMountSdCard();
                                }
                            }
                        }
                        FSADelClient(fsaHandle);
                    }
                }
                break;
            }
            if (pressedBack()) return;
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

    FatMountGuard guard;
    while (true) {
        CHECK_SHUTDOWN();
        if (!waitForDevice(fsaHandle, L"SD card", guard)) {
            break;
        }

        FSADeviceInfo deviceInfo;
        status = (int)FSAGetDeviceInfo(fsaHandle, "/dev/sdcard01", &deviceInfo);
        if ((FSStatus)status != FS_STATUS_OK) {
            WHBLogPrintf("FSAGetDeviceInfo failed: %d", status);
            WHBLogFreetypeDraw();
            setErrorPrompt(L"Failed to get device info!");
            if (!showErrorPrompt(L"Cancel", true)) {
                break;
            }
            continue;
        }

        showDeviceInfoScreen(fsaHandle, "/dev/sdcard01", deviceInfo);
        if (showDialogPrompt(L"Is this the correct device?", L"Yes", L"No", nullptr, nullptr, 1, false) != 0) {
            continue;
        }

        if (handleSDUSBAction(fsaHandle, deviceInfo, guard)) {
            FSADelClient(fsaHandle);
            performPostSetupChecks(false, true);
            return;
        }
    }

    FSADelClient(fsaHandle);
}

void showUSBPartitionMenu() {
    uint8_t selectedOption = 0;
    while (true) {
        CHECK_SHUTDOWN();
        std::string pluginPath;
        bool hasEmulation = false;
        bool pluginInstalled = getInstalledUSBPartitionPlugin(pluginPath, hasEmulation);

        WHBLogFreetypeStartScreen();
        WHBLogFreetypePrint(L"USB Partition Menu");
        WHBLogFreetypePrint(L"===============================");
        WHBLogFreetypePrintf(L"%C Set up USB Partition", (selectedOption == 0 ? L'>' : L' '));
        WHBLogFreetypePrintf(L"%C Uninstall USB Partition", (selectedOption == 1 ? L'>' : L' '));
        WHBLogFreetypePrintf(L"%C Unpartition USB drive (zero out MBR)", (selectedOption == 2 ? L'>' : L' '));

        uint8_t maxOption = 2;
        if (pluginInstalled) {
            maxOption = 3;
            WHBLogFreetypePrintf(L"%C %ls SD emulation", (selectedOption == 3 ? L'>' : L' '), (hasEmulation ? L"Disable" : L"Enable"));
        }

        WHBLogFreetypePrint(L" ");
        WHBLogFreetypeScreenPrintBottom(L"===============================");
        WHBLogFreetypeScreenPrintBottom(L"\uE000 Button = Select Option \uE001 Button = Back");
        WHBLogFreetypeDrawScreen();

        updateInputs();
        while (true) {
            updateInputs();
            if (navigatedUp() && selectedOption > 0) {
                selectedOption--;
                break;
            }
            if (navigatedDown() && selectedOption < maxOption) {
                selectedOption++;
                break;
            }
            if (pressedOk()) {
                if (selectedOption == 0) {
                    setupPartitionedUSBMenu();
                } else if (selectedOption == 1) {
                    uninstallUSBPartition();
                } else if (selectedOption == 2) {
                    // Zero out USB MBR
                    FSAClientHandle fsaHandle = FSAAddClient(NULL);
                    if (fsaHandle >= 0) {
                        usbAsSd(true);
                        WHBUnmountSdCard();
                        FatMountGuard guard;
                        if (waitForDevice(fsaHandle, L"USB device", guard)) {
                            FSADeviceInfo deviceInfo;
                            if ((FSStatus)FSAGetDeviceInfo(fsaHandle, "/dev/sdcard01", &deviceInfo) == FS_STATUS_OK) {
                                if (showDialogPrompt(L"WARNING: This will DELETE the MBR and ALL partition information on the USB device.\nDo you want to continue?", L"Yes", L"No", nullptr, nullptr, 1, false) == 0) {
                                    deleteMbr(fsaHandle, "/dev/sdcard01", deviceInfo);
                                }
                            }
                        }
                        FSADelClient(fsaHandle);
                        usbAsSd(false);
                    }
                } else if (selectedOption == 3) {
                    bool targetEmulation = !hasEmulation;
                    if (targetEmulation && pluginPath != Paths::SlcPluginsDir) {
                        if (showDialogPrompt(L"To use SD Emulation, stroopwafel needs to be installed to the SLC", L"OK", L"Abort", nullptr, nullptr, 1, false) != 0) {
                            break;
                        }
                        removeFile(pluginPath + (hasEmulation ? "/5upartsd.ipx" : "/5usbpart.ipx"));
                        pluginPath = Paths::SlcPluginsDir;
                    } else {
                        removeFile(pluginPath + (hasEmulation ? "/5upartsd.ipx" : "/5usbpart.ipx"));
                    }

                    setStroopwafelPluginPath(pluginPath);
                    if (downloadUsbPartitionPlugin(targetEmulation)) {
                        std::wstring msg = L"SD emulation " + std::wstring(targetEmulation ? L"enabled" : L"disabled") + L" successfully.";
                        showSuccessPrompt(msg.c_str());
                        if (targetEmulation) {
                            performAromaCheck();
                        }
                    }
                }
                break;
            }
            if (pressedBack()) return;
        }
    }
}

void setupPartitionedUSBMenu() {
    uint8_t emulationChoice = showDialogPrompt(L"Do you want to use the first FAT32 partition as an emulated SD card?\nThis should ONLY be used if you don't intend to use a real SD card.", L"Yes (SD Emulation)", L"No", L"Cancel", nullptr, 1);
    if (emulationChoice == 2 || emulationChoice == 255) return;

    std::string pluginTarget = getStroopwafelPluginPath();
    bool sdEmulation = (emulationChoice == 0);

    if(sdEmulation && pluginTarget != Paths::SlcPluginsDir){
        if(showDialogPrompt(L"To use SD Emulation, stroopwafel needs to be installed to the SLC", L"OK", L"Abort", nullptr, nullptr, 1, false) != 0){
            return;
        }
        setStroopwafelPluginPath(Paths::SlcPluginsDir);
    }

    while (pluginTarget.empty() || !dirExist(pluginTarget)) {
        if (showDialogPrompt(L"Stroopwafel is not detected. It is required for partitioned USB storage.\nDo you want to install it now?", L"Yes", L"Cancel") != 0) return;
        
        if (installStroopwafel()) {
            pluginTarget = getStroopwafelPluginPath();
        }
    }

    if(!downloadUsbPartitionPlugin(sdEmulation))
        return;

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

    usbAsSd(true);

    WHBLogFreetypePrint(L"Unmounting USB devices...");
    WHBLogFreetypeDrawScreen();
    int status = WHBUnmountSdCard();
    if (status != 1) {
        WHBLogPrintf("Unmount failed (status: %d), ignoring...", status);
        WHBLogFreetypeDraw();
    }

    FatMountGuard guard;
    bool partitioned = false;
    do {
        CHECK_SHUTDOWN();
        if (!waitForDevice(fsaHandle, L"USB device", guard)) {
            goto exit;
        }

        FSADeviceInfo deviceInfo;
        status = (int)FSAGetDeviceInfo(fsaHandle, "/dev/sdcard01", &deviceInfo);
        if ((FSStatus)status != FS_STATUS_OK) {
            WHBLogPrintf("FSAGetDeviceInfo failed: %d", status);
            WHBLogFreetypeDraw();
            setErrorPrompt(L"Failed to get device info!");
            if (!showErrorPrompt(L"Cancel", true)) {
                goto exit;
            }
            continue;
        }

        showDeviceInfoScreen(fsaHandle, "/dev/sdcard01", deviceInfo);
        if (showDialogPrompt(L"Is this the correct device?", L"Yes", L"No", nullptr, nullptr, 1, false) != 0) {
            continue;
        }

        checkAndFixPartitionOrder(fsaHandle, "/dev/sdcard01", deviceInfo, partitioned);

        if(!partitioned) {
            partitioned = handlePartitionActionMenu(fsaHandle, deviceInfo, L"USB device", !sdEmulation);
        }
    } while (!partitioned);

    if(sdEmulation){
        guard.unblock();
        WHBMountSdCard();
        performAromaCheck();
    } else {
        guard.silent_unblock();
    }

    if(!performIsfshaxCheck(true, false)){
        goto exit;
    }

    if(!sdEmulation){
        if (showDialogPrompt(L"USB partitioned successfully!\nIt is recommended to shutdown the console now\nand plug your SD card back in.\nDo you want to shutdown now?", L"Yes", L"No") == 0) {
            setShutdownPending(true, true);
        } else {
            setShutdownPending(true, false);
        }
    }

exit:
    usbAsSd(false);
    FSADelClient(fsaHandle);
}
