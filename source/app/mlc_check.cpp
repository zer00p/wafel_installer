#include "mlc_check.h"
#include "menu.h"
#include "gui.h"
#include "navigation.h"
#include "common_paths.h"
#include "cfw.h"
#include <mocha/mocha.h>
#include <mocha/fsa.h>
#include <dirent.h>
#include <fstream>
#include <string>
#include <malloc.h>

/*
 * This file contains code and logic inspired by the following open source projects:
 * - Manufacturer checking: WiiUIdent by GaryOderNichts (https://github.com/GaryOderNichts/WiiUIdent)
 */
struct SALDeviceParams {
    uint32_t usrptr;
    uint32_t mid_prv;
    uint32_t device_type;
    uint8_t unknown_1c[0x1c];
    uint64_t numBlocks;
    uint32_t blockSize;
    uint8_t unknown_18[0x18];
    char name0[128];
    char name1[128];
    char name2[128];
    uint32_t functions[12];
} __attribute__((packed));

struct MDBlkDrv {
    int32_t registered;
    uint8_t unknown_8[0x8];
    SALDeviceParams params;
    int sal_handle;
    int deviceId;
    uint8_t unknown_c4[0xc4];
} __attribute__((packed));

static void checkMlcManufacturer(bool& isHynix, std::wstring& manufacturerName) {
    isHynix = false;
    manufacturerName = L"Unknown";

    constexpr uint32_t MDBLK_DRIVER_ADDRESS = 0x11c39e78;
    MDBlkDrv blkDrvs[2]{};
    for (uint32_t i = 0; i < sizeof(blkDrvs) / 4; i++) {
        Mocha_IOSUKernelRead32(MDBLK_DRIVER_ADDRESS + (i * 4), ((uint32_t*) &blkDrvs) + i);
    }

    for (const auto& drv : blkDrvs) {
        if (!drv.registered) continue;
        uint32_t mid = drv.params.mid_prv >> 16;
        
        // Use the first valid one we find, which should be the MLC
        if (mid == 0x90) {
            isHynix = true;
            manufacturerName = L"Hynix";
            return;
        } else if (mid == 0x11) {
            manufacturerName = L"Toshiba";
            return;
        } else if (mid == 0x15) {
            manufacturerName = L"Samsung";
            return;
        } else if (mid != 0) {
            // Include MID in hex for debugging
            wchar_t midHex[16];
            swprintf(midHex, sizeof(midHex) / sizeof(wchar_t), L"Unknown (%02X)", mid);
            manufacturerName = midHex;
            return;
        }
    }
}

static void checkMlcLogs(int& corruptionErrors, int& mediaErrors) {
    corruptionErrors = 0;
    mediaErrors = 0;

    std::string sourceDirectory = Paths::SlcRoot + "/sys/logs";
    DIR *dir = opendir(sourceDirectory.c_str());
    if (!dir) {
        sourceDirectory = "/vol/system/sys/logs";
        dir = opendir(sourceDirectory.c_str());
    }
    
    if (!dir) {
        return;
    }

    struct dirent *dp;
    while ((dp = readdir(dir)) != nullptr) {
        std::string filename(dp->d_name);
        if (filename.length() >= 4 && filename.substr(filename.length() - 4) == ".log") {
            std::string sourceFullPath = sourceDirectory + "/" + filename;
            std::ifstream file(sourceFullPath);
            std::string line;
            while (std::getline(file, line)) {
                if (line.find("DATA CORRUPTION ERROR") != std::string::npos) {
                    size_t idx = line.find("dev:");
                    if (idx != std::string::npos && line.find("mlc", idx) != std::string::npos) {
                        corruptionErrors++;
                    }
                }
                if (line.find("MEDIA ERROR") != std::string::npos) {
                    size_t idx = line.find("dev:");
                    if (idx != std::string::npos && line.find("mlc", idx) != std::string::npos) {
                        mediaErrors++;
                    }
                }
            }
        }
    }
    closedir(dir);
}

static int testReadMlcRaw() {
    uint32_t original_FAE8 = 0;
    uint32_t original_FAEC = 0;
    FSAClientHandle fsaHandle = -1;
    IOSHandle handle = -1;
    int errorCount = -1;
    uint8_t* buffer = nullptr;
    uint32_t buffer_size_lba = 128;
    size_t buffer_size = 0;
    uint64_t totalBlocks = 0;
    uint64_t blocksRead = 0;
    FSADeviceInfo deviceInfo;

    Mocha_IOSUKernelRead32(0x1070FAE8, &original_FAE8);
    Mocha_IOSUKernelRead32(0x1070FAEC, &original_FAEC);

    // Patch FSA raw access to bypass Aroma's block
    Mocha_IOSUKernelWrite32(0x1070FAE8, 0x05812070);
    Mocha_IOSUKernelWrite32(0x1070FAEC, 0xEAFFFFF9);

    fsaHandle = FSAAddClient(nullptr);
    if (fsaHandle < 0) {
        showDialogPrompt(L"Failed to add FSA client.", L"OK");
        goto cleanup;
    }

    if ((FSStatus)FSAGetDeviceInfo(fsaHandle, "/dev/mlc01", &deviceInfo) != FS_STATUS_OK) {
        showDialogPrompt(L"Failed to get device info for /dev/mlc01.", L"OK");
        goto cleanup;
    }

    buffer_size = deviceInfo.deviceSectorSize * buffer_size_lba;
    totalBlocks = deviceInfo.deviceSizeInSectors;

    if (FSAEx_RawOpenEx(fsaHandle, "/dev/mlc01", &handle) < 0) {
        showDialogPrompt(L"Failed to open /dev/mlc01 for raw reading.\nDevice might still be mounted.", L"OK");
        goto cleanup;
    }

    buffer = (uint8_t*)memalign(0x40, buffer_size);
    if (!buffer) {
        goto cleanup;
    }

    errorCount = 0;

    WHBLogFreetypeStartScreen();
    WHBLogFreetypePrint(L"Reading MLC...");
    WHBLogFreetypeDraw();

    while (blocksRead < totalBlocks) {
        if (isShutdownForced()) {
            errorCount = -1;
            goto cleanup;
        }

        uint32_t blocksToRead = buffer_size_lba;
        if (blocksRead + blocksToRead > totalBlocks) {
            blocksToRead = totalBlocks - blocksRead;
        }

        FSStatus res = (FSStatus)FSAEx_RawReadEx(fsaHandle, buffer, deviceInfo.deviceSectorSize, blocksToRead, blocksRead, handle);
        if (res != FS_STATUS_OK) {
            errorCount++;
        }

        blocksRead += blocksToRead;

        if ((blocksRead % (buffer_size_lba * 20)) == 0 || blocksRead == totalBlocks) {
            WHBLogFreetypeStartScreen();
            WHBLogFreetypePrint(L"Test reading raw MLC...");
            std::wstring progress = L"Progress: " + std::to_wstring(blocksRead / 2048) + L" MB / " + std::to_wstring(totalBlocks / 2048) + L" MB";
            WHBLogFreetypePrint(progress.c_str());
            std::wstring errs = L"Errors encountered: " + std::to_wstring(errorCount);
            WHBLogFreetypePrint(errs.c_str());
            WHBLogFreetypePrint(L" ");
            WHBLogFreetypePrint(L"Press B to abort test");
            WHBLogFreetypeDrawScreen();

            updateInputs();
            if (pressedBack()) {
                if(!errorCount)
                    errorCount = -2; // Aborted
                goto cleanup;
            }
        }
    }

cleanup:
    if (buffer) {
        free(buffer);
    }
    if (handle >= 0) {
        FSAEx_RawCloseEx(fsaHandle, handle);
    }
    if (fsaHandle >= 0) {
        FSADelClient(fsaHandle);
    }
    Mocha_IOSUKernelWrite32(0x1070FAE8, original_FAE8);
    Mocha_IOSUKernelWrite32(0x1070FAEC, original_FAEC);

    return errorCount;
}

void showCheckMlcMenu() {
    WHBLogFreetypeStartScreen();
    WHBLogFreetypePrint(L"Checking Manufacturer & Logs...");
    WHBLogFreetypeDraw();

    bool isHynix = false;
    std::wstring manufacturerName;
    checkMlcManufacturer(isHynix, manufacturerName);

    int corruptionErrors = 0;
    int mediaErrors = 0;
    checkMlcLogs(corruptionErrors, mediaErrors);

    std::wstring report = L"Manufacturer: " + manufacturerName + (isHynix ? L" (WARNING)" : L" (Good)");
    report += L"\nLogs - Data Corruption Errors: " + std::to_wstring(corruptionErrors);
    report += L"\nLogs - Media Errors: " + std::to_wstring(mediaErrors) + L"\n\n";

    bool suggestIsfshax = false;
    bool suggestLongRead = false;

    if (mediaErrors > 0) {
        report += L"A media error means the eMMC is definitely bad.\n";
        suggestIsfshax = true;
    } else {
        if (isHynix) {
            report += L"Hynix eMMCs are known to be more prone to failure.\n";
            suggestLongRead = true;
            suggestIsfshax = true;
        }
        if (corruptionErrors > 0) {
            report += L"Data corruption could mean failure or just a crash.\n";
            suggestIsfshax = true;
            suggestLongRead = true;
        }

        if (!isHynix && corruptionErrors == 0) {
            report += L"No issues detected in logs and manufacturer is fine.\n";
        }
    }

    if (suggestIsfshax && !isIsfshaxInstalled()) {
        report += L"\nCRITICAL: You should install ISFShax to have a safety\n";
        report += L"net for an impending brick!\n";
    }

    showDialogPrompt(report.c_str(), L"OK");

    if (mediaErrors == 0) {
        std::wstring readPrompt = L"Do you want to run the extended raw read test?\n(Takes several hours). ";
        if (!suggestLongRead) {
            readPrompt += L"It is likely not necessary.";
        } else {
            readPrompt += L"It is suggested.";
        }
        
        uint8_t choice = showDialogPrompt(readPrompt.c_str(), L"Yes", L"No", nullptr, nullptr, suggestLongRead ? 0 : 1);
        if (choice == 0) {
            int errors = testReadMlcRaw();
            if (errors == -2) {
                showDialogPrompt(L"Extended read test aborted.", L"OK");
            } else if (errors > 0) {
                std::wstring msg = L"Finished reading MLC.\nFound " + std::to_wstring(errors) + L" read errors.\nThis indicates a failing eMMC.";
                if (!isIsfshaxInstalled()) {
                    msg += L"\n\nCRITICAL: You should install ISFShax immediately!";
                }
                showDialogPrompt(msg.c_str(), L"OK");
            } else if (errors == 0) {
                showSuccessPrompt(L"Finished reading MLC!\nNo media errors found.");
            } else {
                showErrorPrompt(L"Failed to execute raw read test.", false);
            }
        }
    }
}
