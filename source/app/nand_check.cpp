#include "nand_check.h"
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
#include <vector>
#include <malloc.h>
#include <cerrno>
#include "filesystem.h"

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

struct NandLogErrors {
    int mlcCorruptionErrors = 0;
    int mlcMediaErrors = 0;
    int slcCorruptionErrors = 0;
    int slcMediaErrors = 0;
    int slccmptCorruptionErrors = 0;
    int slccmptMediaErrors = 0;
};

static bool checkNandLogs(NandLogErrors& errors) {
    errors = {};

    std::string sourceDirectory = "/vol/system/logs";
    DIR *dir = dirOpen(sourceDirectory);
    
    if (!dir) {
        return false;
    }

    struct dirent *dp;
    while ((dp = readdir(dir)) != nullptr) {
        std::string filename(dp->d_name);
        if (filename.length() >= 4 && filename.substr(filename.length() - 4) == ".log") {
            std::string sourceFullPath = sourceDirectory + "/" + filename;
            std::ifstream file(convertToWiiUFsPath(sourceFullPath));
            std::string line;
            while (std::getline(file, line)) {
                if (line.find("DATA CORRUPTION ERROR") != std::string::npos) {
                    size_t idx = line.find("dev:");
                    if (idx != std::string::npos) {
                        std::string devPart = line.substr(idx);
                        // Check slccmpt first (longer match) before slc
                        if (devPart.find("slccmpt") != std::string::npos) {
                            errors.slccmptCorruptionErrors++;
                        } else if (devPart.find("slc") != std::string::npos) {
                            errors.slcCorruptionErrors++;
                        } else if (devPart.find("mlc") != std::string::npos) {
                            errors.mlcCorruptionErrors++;
                        }
                    }
                }
                if (line.find("MEDIA ERROR") != std::string::npos) {
                    size_t idx = line.find("dev:");
                    if (idx != std::string::npos) {
                        std::string devPart = line.substr(idx);
                        if (devPart.find("slccmpt") != std::string::npos) {
                            errors.slccmptMediaErrors++;
                        } else if (devPart.find("slc") != std::string::npos) {
                            errors.slcMediaErrors++;
                        } else if (devPart.find("mlc") != std::string::npos) {
                            errors.mlcMediaErrors++;
                        }
                    }
                }
            }
        }
    }
    closedir(dir);
    return true;
}

static void readFilesRecursive(const std::string& dirPath, std::vector<std::string>& corruptedFiles, int& filesScanned, const std::wstring& partitionName, bool& aborted) {
    if (aborted || isShutdownForced()) {
        aborted = true;
        return;
    }

    DIR *dir = dirOpen(dirPath);
    if (!dir) {
        return;
    }

    struct dirent *dp;
    while ((dp = readdir(dir)) != nullptr) {
        if (aborted || isShutdownForced()) {
            aborted = true;
            break;
        }

        std::string name(dp->d_name);
        if (name == "." || name == "..") continue;

        std::string fullPath = dirPath + "/" + name;

        if ((dp->d_type & DT_DIR) == DT_DIR) {
            readFilesRecursive(fullPath, corruptedFiles, filesScanned, partitionName, aborted);
        } else {
            // Try to open and read the file
            std::string convertedPath = convertToWiiUFsPath(fullPath);
            std::ifstream file(convertedPath, std::ios::binary);
            if (!file.is_open()) {
                // File couldn't be opened — likely held open by the system (EACCES/EBUSY)
                // Don't count as an error
                continue;
            }

            char buffer[4096];
            bool readError = false;
            while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
                if (file.bad()) {
                    readError = true;
                    break;
                }
                if (file.gcount() == 0) break;
            }
            if (file.bad()) {
                readError = true;
            }
            file.close();

            if (readError) {
                corruptedFiles.push_back(fullPath);
            }

            filesScanned++;

            // Update progress periodically
            if ((filesScanned % 50) == 0) {
                WHBLogFreetypeStartScreen();
                WHBLogFreetypePrintf(L"Scanning %S...", partitionName.c_str());
                WHBLogFreetypePrintf(L"Files scanned: %d", filesScanned);
                if (!corruptedFiles.empty()) {
                    WHBLogFreetypePrintf(L"Corrupted files found: %d", (int)corruptedFiles.size());
                }
                WHBLogFreetypePrint(L" ");
                WHBLogFreetypePrint(L"Press B to abort");
                WHBLogFreetypeDrawScreen();

                updateInputs();
                if (pressedBack()) {
                    aborted = true;
                    break;
                }
            }
        }
    }
    closedir(dir);
}

struct SlcCheckResult {
    std::vector<std::string> corruptedFiles;
    int filesScanned = 0;
    bool aborted = false;
    bool accessible = false;
};

static SlcCheckResult checkSlcFiles(const std::string& volPath, const std::wstring& partitionName) {
    SlcCheckResult result;

    if (!dirExist(volPath)) {
        result.accessible = false;
        return result;
    }
    result.accessible = true;

    WHBLogFreetypeStartScreen();
    WHBLogFreetypePrintf(L"Scanning %S...", partitionName.c_str());
    WHBLogFreetypeDraw();

    readFilesRecursive(volPath, result.corruptedFiles, result.filesScanned, partitionName, result.aborted);

    return result;
}

static int testReadMlcRaw() {
    uint32_t original_FAE8 = 0;
    uint32_t original_FAEC = 0;
    FSAClientHandle fsaHandle = -1;
    IOSHandle handle = -1;
    int errorCount = -1;
    uint8_t* buffer = nullptr;
    uint32_t buffer_size_lba = 2048; // 1MB
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
        showDialogPrompt(L"Failed to open /dev/mlc01 for raw reading.", L"OK");
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
        }

        updateInputs();
        if (pressedBack()) {
            if(!errorCount)
                errorCount = -2; // Aborted
            goto cleanup;
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

static void showMlcAlternatives() {
    std::wstring msg =
        L"Your eMMC (MLC) appears to be failing.\n"
        L"Consider one of these alternatives:\n \n"
        L"- redNAND: Uses your SD card as replacement\n"
        L"  https://gbatemp.net/threads/642268/\n \n"
        L"- USBMLC: Uses a USB device as replacement\n"
        L"  https://gbatemp.net/threads/672971/\n";

    if (!isIsfshaxInstalled()) {
        msg += L"\nCRITICAL: Install ISFShax first to prevent a brick!\n";
    }

    showDialogPrompt(msg.c_str(), L"OK");
}

static void showSlcIssuesReport(const std::wstring& partitionLabel, const SlcCheckResult& fileResult,
                                int logCorruptionErrors, int logMediaErrors) {
    std::wstring msg = L"Issues found on " + partitionLabel + L".\n";

    if (logCorruptionErrors > 0) {
        msg += L"Log corruption errors: " + std::to_wstring(logCorruptionErrors) + L"\n";
    }
    if (logMediaErrors > 0) {
        msg += L"Log media errors: " + std::to_wstring(logMediaErrors) + L"\n";
    }

    msg += L"This is NOT the usual eMMC (MLC) failure.\n";
    msg += L"Please ask for assistance before taking action.\n";

    if (!fileResult.corruptedFiles.empty()) {
        msg += L"\nCorrupted files:\n";
        // Show up to 10 corrupted files to avoid overflowing the screen
        int shown = 0;
        for (const auto& path : fileResult.corruptedFiles) {
            if (shown >= 10) {
                msg += L"  ... and " + std::to_wstring(fileResult.corruptedFiles.size() - 10) + L" more\n";
                break;
            }
            // Convert vol path to a shorter display path
            std::wstring wpath(path.begin(), path.end());
            msg += L"  " + wpath + L"\n";
            shown++;
        }
    }

    showDialogPrompt(msg.c_str(), L"OK");
}

void showCheckNandMenu() {
    WHBLogFreetypeStartScreen();
    WHBLogFreetypePrint(L"Checking NAND Health...");
    WHBLogFreetypeDraw();

    // --- Phase 1: Manufacturer check ---
    bool isHynix = false;
    std::wstring manufacturerName;
    checkMlcManufacturer(isHynix, manufacturerName);

    // --- Phase 2: Log check ---
    NandLogErrors logErrors;
    bool logsSuccess = checkNandLogs(logErrors);

    // --- Phase 3: SLC file read test ---
    uint8_t scanSlc = showDialogPrompt(
        L"Do you want to run a file read test on the SLC?\n(Takes a few minutes, usually not necessary)",
        L"Yes", L"No", nullptr, nullptr, 1
    );
    SlcCheckResult slcFileResult;
    if (scanSlc == 0) {
        slcFileResult = checkSlcFiles("/vol/system", L"SLC");
    }

    // --- Phase 4: SLCCMPT file read test ---
    uint8_t scanSlccmpt = 1;
    SlcCheckResult slccmptFileResult;
    bool slccmptMounted = false;
    if (USE_LIBMOCHA()) {
        scanSlccmpt = showDialogPrompt(
            L"Do you want to run a file read test on SLCCMPT?\n(Takes a few minutes, usually not necessary)",
            L"Yes", L"No", nullptr, nullptr, 1
        );
        if (scanSlccmpt == 0) {
            WHBLogFreetypeStartScreen();
            WHBLogFreetypePrint(L"Mounting SLCCMPT...");
            WHBLogFreetypeDraw();
            if (Mocha_MountFS("storage_slccmpt01", "/dev/slccmpt01", "/vol/storage_slccmpt01") == MOCHA_RESULT_SUCCESS) {
                slccmptMounted = true;
                slccmptFileResult = checkSlcFiles("/vol/storage_slccmpt01", L"SLCCMPT");
                Mocha_UnmountFS("storage_slccmpt01");
            }
        }
    }

    // --- Build and display MLC report ---
    std::wstring report = L"=== MLC (eMMC) ===\n";
    report += L"Manufacturer: " + manufacturerName + (isHynix ? L" (WARNING)" : L" (Good)");

    if (logsSuccess) {
        report += L"\nCorruption errors in logs: " + std::to_wstring(logErrors.mlcCorruptionErrors);
        report += L"\nMedia errors in logs: " + std::to_wstring(logErrors.mlcMediaErrors);
    } else {
        report += L"\nLogs: Could not open logs directory";
    }

    // SLC/SLCCMPT summary in initial report
    bool slcLogIssues = (logErrors.slcCorruptionErrors > 0 || logErrors.slcMediaErrors > 0);
    bool slccmptLogIssues = (logErrors.slccmptCorruptionErrors > 0 || logErrors.slccmptMediaErrors > 0);

    bool slcHasIssues = slcLogIssues || !slcFileResult.corruptedFiles.empty();
    bool slccmptHasIssues = slccmptLogIssues || !slccmptFileResult.corruptedFiles.empty();

    if (slcHasIssues || slccmptHasIssues) {
        report += L"\n \n";
        if (slcHasIssues) report += L"SLC: Issues detected (details follow)\n";
        if (slccmptHasIssues) report += L"SLCCMPT: Issues detected (details follow)\n";
    } else {
        report += L"\n \n";
        if (scanSlc == 0) {
            report += L"SLC: OK";
        } else {
            report += L"SLC: Log check OK (file scan skipped)";
        }

        if (slccmptMounted && scanSlccmpt == 0) {
            report += L"\nSLCCMPT: OK";
        } else if (scanSlccmpt == 0 && USE_LIBMOCHA()) {
            report += L"\nSLCCMPT: Could not mount";
        } else {
            report += L"\nSLCCMPT: Log check OK (file scan skipped)";
        }
    }

    report += L"\n";

    // --- Decision logic ---
    bool mlcConfirmedBad = false;
    bool suggestLongRead = false;
    bool slcCorruptionOverride = false;

    if (slcLogIssues) {
        // Only SLC log errors override MLC guidance
        slcCorruptionOverride = true;
        report += L"\nSLC log errors detected. MLC errors may be a side effect\n";
        report += L"of SLC corruption (write cache). Ask for assistance.\n";
    }

    if (logErrors.mlcMediaErrors > 0) {
        mlcConfirmedBad = true;
        report += L"\nMedia errors confirm the eMMC is failing.\n";
    } else if (logErrors.mlcCorruptionErrors > 0) {
        report += L"\nData corruption could mean failure or just a crash.\n";
        suggestLongRead = true;
    }

    if (isHynix && !mlcConfirmedBad) {
        report += L"Hynix eMMCs are more prone to failure.\n";
        suggestLongRead = true;
    }

    if (!slcCorruptionOverride && !mlcConfirmedBad && !isHynix && logErrors.mlcCorruptionErrors == 0) {
        report += L"\nNo issues detected.\n";
    }

    if (!slcCorruptionOverride && (mlcConfirmedBad || suggestLongRead) && !isIsfshaxInstalled()) {
        report += L"\nCRITICAL: You should install ISFShax as a safety\n";
        report += L"net against an impending brick!\n";
    }

    showDialogPrompt(report.c_str(), L"OK");

    // --- Show SLC/SLCCMPT detailed reports ---
    if (slcHasIssues) {
        showSlcIssuesReport(L"SLC (system storage)", slcFileResult,
                           logErrors.slcCorruptionErrors, logErrors.slcMediaErrors);
    }
    if (slccmptHasIssues) {
        showSlcIssuesReport(L"SLCCMPT (vWii storage)", slccmptFileResult,
                           logErrors.slccmptCorruptionErrors, logErrors.slccmptMediaErrors);
    }

    // --- MLC confirmed bad via logs: show alternatives, no raw read ---
    if (mlcConfirmedBad && !slcCorruptionOverride) {
        showMlcAlternatives();
        return;
    }

    // --- Offer raw read test ---
    std::wstring readPrompt = L"Do you want to run the extended raw read test?\n(Takes several hours). ";
    if (slcCorruptionOverride) {
        readPrompt += L"\nNote: SLC issues were detected. MLC results\nmay be misleading.";
    } else if (!suggestLongRead) {
        readPrompt += L"It is likely not necessary.";
    } else {
        readPrompt += L"It is suggested.";
    }

    uint8_t defaultOpt = suggestLongRead ? 0 : 1;
    uint8_t choice = showDialogPrompt(readPrompt.c_str(), L"Yes", L"No", nullptr, nullptr, defaultOpt);
    if (choice == 0) {
        int rawErrors = testReadMlcRaw();
        if (rawErrors == -2) {
            showDialogPrompt(L"Extended read test aborted.", L"OK");
        } else if (rawErrors > 0) {
            std::wstring msg = L"Finished reading MLC.\nFound " + std::to_wstring(rawErrors) + L" read errors.\nThis indicates a failing eMMC.";
            if (!isIsfshaxInstalled()) {
                msg += L"\n\nCRITICAL: You should install ISFShax immediately!";
            }
            showDialogPrompt(msg.c_str(), L"OK");

            if (!slcCorruptionOverride) {
                showMlcAlternatives();
            } else {
                showDialogPrompt(
                    L"SLC issues were also detected.\n"
                    L"MLC errors may be related to SLC corruption.\n"
                    L"Please ask for assistance before using redNAND\n"
                    L"or other MLC replacements.",
                    L"OK");
            }
        } else if (rawErrors == 0) {
            showSuccessPrompt(L"Finished reading MLC!\nNo media errors found.");
        } else {
            showErrorPrompt(L"Failed to execute raw read test.", false);
        }
    }
}
