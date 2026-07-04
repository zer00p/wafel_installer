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
#include "isfshax_menu.h"
#include "pluginmanager.h"
#include "system_hashes_data.h"
#include "../utils/sha256.h"
#include <sstream>
#include <iomanip>

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
    bool scfmCorruption = false;
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
                if (line.find("FSAReadFileFromSLC") != std::string::npos ||
                    line.find("FSAWriteFileToSLC") != std::string::npos ||
                    line.find("readDataFromCache() FSA resource error") != std::string::npos) {
                    errors.scfmCorruption = true;
                }
            }
        }
    }
    closedir(dir);
    return true;
}

static void readFilesRecursive(const std::string& dirPath, std::vector<std::string>& corruptedFiles, int& filesScanned, const std::wstring& partitionName, bool& aborted) {
    DIR *dir = dirOpen(dirPath);
    if (!dir) {
        return;
    }

    struct dirent *dp;
    while ((dp = readdir(dir)) != nullptr && !aborted) {
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

            // Update progress
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
        L"Consider one of these fixes:\n \n"
        L"- redNAND: Uses your SD card as replacement\n"
        L"  https://gbatemp.net/threads/642268/\n \n"
        L"- USBMLC: Uses a USB device as replacement\n"
        L"  https://gbatemp.net/threads/672971/\n";

    if (!isIsfshaxInstalled()) {
        msg += L"\nCRITICAL: Install ISFShax first to prevent a brick!\n";
    }

    showDialogPrompt(msg.c_str(), L"OK");
}

static void showSlcIssuesReport(const std::wstring& partitionLabel, const SlcCheckResult& fileResult) {
    if (!fileResult.accessible) {
        std::wstring msg = L"Failed to access " + partitionLabel + L".\nCould not perform read test.";
        setErrorPrompt(msg);
        showErrorPrompt(L"OK", false);
        return;
    }

    if (fileResult.aborted) {
        std::wstring msg = L"Read test for " + partitionLabel + L" was aborted.\nFiles scanned: " + std::to_wstring(fileResult.filesScanned);
        showDialogPrompt(msg.c_str(), L"OK");
        return;
    }

    if (fileResult.corruptedFiles.empty()) {
        std::wstring msg = L"Read test for " + partitionLabel + L" completed successfully.\nFiles scanned: " + std::to_wstring(fileResult.filesScanned) + L"\nNo corrupted files were found.";
        showSuccessPrompt(msg.c_str());
        return;
    }
    
    std::wstring msg = std::to_wstring(fileResult.filesScanned) + L" files scanned.\nCorrupted files found on " + partitionLabel + L":\n";
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

    bool slcLogIssues = (logErrors.slcCorruptionErrors > 0 || logErrors.slcMediaErrors > 0 || logErrors.scfmCorruption);
    bool slccmptLogIssues = (logErrors.slccmptCorruptionErrors > 0 || logErrors.slccmptMediaErrors > 0);
    bool mlcLogIssues = (logErrors.mlcCorruptionErrors > 0 || logErrors.mlcMediaErrors > 0);
    bool mlcConfirmedBad = (logErrors.mlcMediaErrors > 0);

    // --- Log Report ---
    std::wstring logReport = L"=== Log Report ===\n";
    logReport += L"MLC Manufacturer: " + manufacturerName + (isHynix ? L" (WARNING)\n" : L" (Good)\n");

    if (logsSuccess) {
        logReport += L"\nMLC Errors:\n";
        logReport += L"Corruption: " + std::to_wstring(logErrors.mlcCorruptionErrors) + L", Media: " + std::to_wstring(logErrors.mlcMediaErrors) + L"\n";
        
        logReport += L"\nSLC Errors:\n";
        if (logErrors.scfmCorruption) {
            logReport += L"SCFM cache is corrupted!\n";
        }
        logReport += L"Corruption: " + std::to_wstring(logErrors.slcCorruptionErrors) + L", Media: " + std::to_wstring(logErrors.slcMediaErrors) + L"\n";
        
        logReport += L"\nSLCCMPT Errors:\n";
        logReport += L"Corruption: " + std::to_wstring(logErrors.slccmptCorruptionErrors) + L", Media: " + std::to_wstring(logErrors.slccmptMediaErrors) + L"\n";

        if (logErrors.scfmCorruption && logErrors.mlcCorruptionErrors > 0) {
            logReport += L"\nNote: The MLC errors detected may be a side effect\n";
            logReport += L"of the SCFM cache corruption on the SLC.\n";
            logReport += L"Please ask for assistance before taking action. (except installing ISFShax)\n";
        } else if (logErrors.scfmCorruption || slcLogIssues || (!mlcLogIssues && slccmptLogIssues)) {
            logReport += L"\nNote: This is NOT the usual eMMC (MLC) failure.\n";
            logReport += L"Please ask for assistance before taking action.\n";
        }
    } else {
        logReport += L"\nCould not open logs directory\n";
    }

    showDialogPrompt(logReport.c_str(), L"OK");

    if (!isIsfshaxInstalled()) {
        if (mlcConfirmedBad) {
            showMlcAlternatives();
        }

        if (logErrors.scfmCorruption || mlcConfirmedBad) {
            if (showDialogPrompt(L"\nCRITICAL: You should install ISFShax IMMEDIATELY\nas a safety net against an impending brick!", L"Yes, install ISFShax", L"No, skip", nullptr, nullptr, 0) == 0) {
                installIsfshax(false, false);
            }
        }
    }

    // --- Phase 3: SLC file read test ---
    std::wstring slcPrompt = L"Do you want to run a file read test on the SLC?\n(Takes a few minutes)";
    uint8_t slcDefault = 1;
    if (slcLogIssues) {
        slcPrompt = L"SLC errors found in logs. Do you want to run a file\nread test on the SLC to see how bad it is?";
        slcDefault = 0;
    }
    uint8_t scanSlc = showDialogPrompt(slcPrompt.c_str(), L"Yes", L"No", nullptr, nullptr, slcDefault);
    SlcCheckResult slcFileResult;
    if (scanSlc == 0) {
        slcFileResult = checkSlcFiles("/vol/system", L"SLC");
        showSlcIssuesReport(L"SLC (system storage)", slcFileResult);
    }

    // --- Phase 4: SLCCMPT file read test ---
    uint8_t scanSlccmpt = 1;
    SlcCheckResult slccmptFileResult;
    if (USE_LIBMOCHA()) {
        std::wstring slccmptPrompt = L"Do you want to run a file read test on SLCCMPT (vWii)?\n(Takes a few minutes)";
        uint8_t slccmptDefault = 1;
        if (slccmptLogIssues) {
            slccmptPrompt = L"SLCCMPT errors found in logs. Do you want to run a file\nread test on SLCCMPT to see how bad it is?";
            slccmptDefault = 0;
        }
        scanSlccmpt = showDialogPrompt(slccmptPrompt.c_str(), L"Yes", L"No", nullptr, nullptr, slccmptDefault);
        if (scanSlccmpt == 0) {
            WHBLogFreetypeStartScreen();
            WHBLogFreetypePrint(L"Mounting SLCCMPT...");
            WHBLogFreetypeDraw();
            if (Mocha_MountFS("storage_slcc01", "/dev/slccmpt01", "/vol/storage_slcc01") == MOCHA_RESULT_SUCCESS) {
                slccmptFileResult = checkSlcFiles("/vol/storage_slcc01", L"SLCCMPT");
                Mocha_UnmountFS("storage_slccmpt01");
            } else {
                slccmptFileResult.accessible = false;
            }
            showSlcIssuesReport(L"SLCCMPT (vWii storage)", slccmptFileResult);
        }
    }

    // --- Offer raw read test ---
    bool suggestLongRead = false;
    if (logErrors.mlcCorruptionErrors > 0 || (isHynix && !mlcConfirmedBad)) {
        suggestLongRead = true;
    }

    std::wstring readPrompt = L"Do you want to run the MLC read test?\n(Takes several hours). ";
    if (mlcConfirmedBad) {
        readPrompt = L"MLC is confirmed failing from logs.\nDo you still want to run the MLC read test?\n(Takes several hours, likely not necessary).";
    } else if (!suggestLongRead) {
        readPrompt += L"It is likely not necessary.";
    } else {
        readPrompt += L"It is recommended.";
    }

    uint8_t defaultOpt = (suggestLongRead && !mlcConfirmedBad) ? 0 : 1;
    uint8_t choice = showDialogPrompt(readPrompt.c_str(), L"Yes", L"No", nullptr, nullptr, defaultOpt);
    
    int rawErrors = 0;
    if (choice == 0) {
        rawErrors = testReadMlcRaw();
        if (rawErrors == -2) {
            showDialogPrompt(L"MLC read test aborted.", L"OK");
        } else if (rawErrors > 0) {
            std::wstring msg = L"Finished reading MLC.\nFound " + std::to_wstring(rawErrors) + L" read errors.\nThis indicates a failing eMMC.";
            showDialogPrompt(msg.c_str(), L"OK");
            mlcConfirmedBad = true; // Mark as bad from raw scan
        } else if (rawErrors == 0) {
            showSuccessPrompt(L"Finished reading MLC!\nNo media errors found.");
            mlcConfirmedBad = false;
        } else {
            showErrorPrompt(L"Failed to execute raw read test.", false);
        }
    }

    // --- Show alternatives if bad ---
    if (mlcConfirmedBad) {
        showMlcAlternatives();
    }

    // --- Offer ISFShax Installation ---
    if (!isIsfshaxInstalled()) {
        std::wstring isfshaxMsg = L"";
        if (logErrors.scfmCorruption || mlcConfirmedBad) {
            isfshaxMsg = L"CRITICAL: You should install ISFShax IMMEDIATELY\nas a safety net against an impending brick!\n\nWould you like to install ISFShax now?";
        } else if (logErrors.slcCorruptionErrors > 0 || logErrors.mlcCorruptionErrors > 0) {
            isfshaxMsg = L"Warning: Corruption errors detected. It is highly\nrecommended to install ISFShax as a precaution.\n\nWould you like to install ISFShax now?";
        } else if (isHynix) {
            isfshaxMsg = L"Note: You have a Hynix eMMC. You can install ISFShax\nnow to allow easy recovery of potential problems in the future.\n\nWould you like to install ISFShax now?";
        }

        if (!isfshaxMsg.empty()) {
            if (showDialogPrompt(isfshaxMsg.c_str(), L"Yes, install ISFShax", L"No, skip", nullptr, nullptr, 0) == 0) {
                installIsfshax(false, false);
            }
        }
    }
}

enum ConsoleRegion {
    REGION_UNKNOWN = 0,
    REGION_JP,
    REGION_US,
    REGION_EU
};

static ConsoleRegion detectConsoleRegion() {
    int regionsFound = 0;
    ConsoleRegion lastRegion = REGION_UNKNOWN;

    if (dirExist("/vol/storage_mlc01/sys/title/00050010/10040000")) {
        regionsFound++;
        lastRegion = REGION_JP;
    }
    if (dirExist("/vol/storage_mlc01/sys/title/00050010/10040100")) {
        regionsFound++;
        lastRegion = REGION_US;
    }
    if (dirExist("/vol/storage_mlc01/sys/title/00050010/10040200")) {
        regionsFound++;
        lastRegion = REGION_EU;
    }

    if (regionsFound == 1) {
        return lastRegion;
    }

    return REGION_UNKNOWN;
}

bool runSystemIntegrityCheck() {
    WHBLogFreetypeStartScreen();
    WHBLogFreetypePrint(L"Running System Integrity Check...");
    WHBLogFreetypeDraw();
    
    if (!isSlcMounted() || !isMlcMounted()) {
        showDialogPrompt(L"System Integrity Check Failed!\nFailed to access system storage.\nPlease make sure to disable 'System Access' in ftpiiu if you have it running.", L"OK");
        return false;
    }
    
    ConsoleRegion region = detectConsoleRegion();

    if (region == REGION_UNKNOWN) {
        showDialogPrompt(L"System Integrity Check Failed!\nMultiple or no region folders found.\nUninstalling is not safe.", L"OK");
        return false;
    }

    // Verify system files
    int filesScanned = 0;
    int filesFailed = 0;
    std::vector<std::string> failedFiles;

    auto scanArray = [&](const SystemFileHashes* hashesArray, size_t numHashes) {
        for (size_t i = 0; i < numHashes; i++) {
            const auto& fileHash = hashesArray[i];
            std::string fullPath = fileHash.filepath;

            filesScanned++;

            WHBLogFreetypeStartScreen();
            WHBLogFreetypePrint(L"Running System Integrity Check...");
            std::wstring scanMsg = L"Files Scanned: " + std::to_wstring(filesScanned);
            WHBLogFreetypePrint(scanMsg.c_str());
            WHBLogFreetypeDrawScreen();

            if (fileExist(fullPath)) {
                std::string actualHashStr = calculateSHA256(fullPath);
                if (actualHashStr.empty()) {
                    failedFiles.push_back(fullPath + " (Read Error)");
                    filesFailed++;
                    continue;
                }
                
                bool match = false;
                for (size_t j = 0; j < fileHash.num_hashes; j++) {
                    if (actualHashStr == fileHash.valid_hashes[j]) {
                        match = true;
                        break;
                    }
                }
                
                if (!match) {
                    failedFiles.push_back(fullPath + " (Modified)");
                    filesFailed++;
                }
            } else {
                failedFiles.push_back(fullPath + " (Missing)");
                filesFailed++;
            }
        }
    };

    scanArray(g_systemFileHashesCommon, g_numSystemFileHashesCommon);
    if (region == REGION_JP) scanArray(g_systemFileHashesJp, g_numSystemFileHashesJp);
    else if (region == REGION_US) scanArray(g_systemFileHashesUs, g_numSystemFileHashesUs);
    else if (region == REGION_EU) scanArray(g_systemFileHashesEu, g_numSystemFileHashesEu);
    
    if (filesFailed > 0) {
        std::wstring msg = L"System Integrity Check Failed!\n" + std::to_wstring(filesFailed) + L" corrupted/modified/missing system files found.\n";
        msg += L"Uninstalling might brick your console.\n";
        int shown = 0;
        for(const auto& f : failedFiles) {
            if (shown >= 5) {
                msg += L"  ... and " + std::to_wstring(failedFiles.size() - 5) + L" more\n";
                break;
            }
            std::wstring wf(f.begin(), f.end());
            msg += L"  " + wf + L"\n";
            shown++;
        }
        showDialogPrompt(msg.c_str(), L"OK");
        return false;
    }
    
    return true;
}


