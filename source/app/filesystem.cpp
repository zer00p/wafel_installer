#include "filesystem.h"
#include "common_paths.h"
#include "cfw.h"
#include "gui.h"
#include "progress.h"

#include <dirent.h>
#include <sys/unistd.h>
#include <sys/statvfs.h>

static bool systemSLCMounted = false;
static bool systemMLCMounted = false;
static bool systemUSBMounted = false;
static bool discMounted = false;

// unmount wii hook
extern "C" FSClient* __wut_devoptab_fs_client;
bool unmountDefaultDevoptab() {
    // Get FS mount path using current directory
    char mountPath[PATH_MAX];
    getcwd(mountPath, sizeof(mountPath));
    memmove(mountPath, mountPath + 3, PATH_MAX - 3);

    FSCmdBlock cmd;
    FSInitCmdBlock(&cmd);
    if (FSStatus res = FSUnmount(__wut_devoptab_fs_client, &cmd, Paths::SdRoot.c_str(), FS_ERROR_FLAG_ALL); res != FS_STATUS_OK) {
        WHBLogPrintf("Couldn't unmount default devoptab with path %s! Error = %X", mountPath, res);
        WHBLogFreetypeDraw();
        return false;
    }

    if (FSStatus res = FSDelClient(__wut_devoptab_fs_client, FS_ERROR_FLAG_ALL); res != FS_STATUS_OK) {
        WHBLogPrintf("Couldn't delete wut devoptab with path %s! Error = %X", mountPath, res);
        WHBLogFreetypeDraw();
        return false;
    }
    return true;
}


bool mountSystemDrives() {
    WHBLogPrint("Mounting system drives...");
    WHBLogFreetypeDraw();
    if (USE_LIBMOCHA()) {
        //unmountDefaultDevoptab();
        if (Mocha_MountFS("storage_slc", "/dev/slc01", "/vol/storage_slc01") == MOCHA_RESULT_SUCCESS) systemSLCMounted = true;
        // if (Mocha_MountFS("storage_mlc01", nullptr, "/vol/storage_mlc01") == MOCHA_RESULT_SUCCESS) systemMLCMounted = true;
        if (Mocha_MountFS("storage_usb01", nullptr, "/vol/storage_usb01") == MOCHA_RESULT_SUCCESS) systemUSBMounted = true;
    }
    else {
        systemMLCMounted = true;
        systemUSBMounted = false;
    }

    if (systemSLCMounted) WHBLogPrint("Successfully mounted the SLC storage!");
    if (systemMLCMounted) WHBLogPrint("Successfully mounted the internal Wii U storage!");
    if (systemUSBMounted) WHBLogPrint("Successfully mounted the external Wii U storage!");
    WHBLogFreetypeDraw();
    return systemSLCMounted; // Require only the MLC to be mounted for this function to be successful
}

bool isSlcMounted() {
    return systemSLCMounted;
}

bool isUsbMounted() {
    return systemUSBMounted;
}

bool mountDisc() {
    if (USE_LIBMOCHA()) {
        if (Mocha_MountFS("storage_odd01", "/dev/odd01", "/vol/storage_odd_tickets") == MOCHA_RESULT_SUCCESS) discMounted = true;
        if (Mocha_MountFS("storage_odd02", "/dev/odd02", "/vol/storage_odd_updates") == MOCHA_RESULT_SUCCESS) discMounted = true;
        if (Mocha_MountFS("storage_odd03", "/dev/odd03", "/vol/storage_odd_content") == MOCHA_RESULT_SUCCESS) discMounted = true;
        if (Mocha_MountFS("storage_odd04", "/dev/odd04", "/vol/storage_odd_content2") == MOCHA_RESULT_SUCCESS) discMounted = true;
    }
    if (discMounted) WHBLogPrint("Successfully mounted the disc!");
    WHBLogFreetypeDraw();
    return discMounted;
}

bool unmountSystemDrives() {
    if (USE_LIBMOCHA()) {
        if (systemSLCMounted && Mocha_UnmountFS("storage_slc") == MOCHA_RESULT_SUCCESS) systemSLCMounted = false;
        if (systemMLCMounted && Mocha_UnmountFS("storage_mlc01") == MOCHA_RESULT_SUCCESS) systemMLCMounted = false;
        if (systemUSBMounted && Mocha_UnmountFS("storage_usb01") == MOCHA_RESULT_SUCCESS) systemUSBMounted = false;
    }
    else {
        systemMLCMounted = false;
        systemUSBMounted = false;
    }
    return (!systemMLCMounted && !systemUSBMounted);
}

bool unmountDisc() {
    if (!discMounted) return false;
    if (USE_LIBMOCHA()) {
        if (Mocha_UnmountFS("storage_odd01") == MOCHA_RESULT_SUCCESS) discMounted = false;
        if (Mocha_UnmountFS("storage_odd02") == MOCHA_RESULT_SUCCESS) discMounted = false;
        if (Mocha_UnmountFS("storage_odd03") == MOCHA_RESULT_SUCCESS) discMounted = false;
        if (Mocha_UnmountFS("storage_odd04") == MOCHA_RESULT_SUCCESS) discMounted = false;
    }
    return !discMounted;
}

bool isDiscMounted() {
    return discMounted;
}

bool testStorage(TITLE_LOCATION location) {
    if (location == TITLE_LOCATION::NAND) return dirExist(Paths::MlcUsrDir);
    if (location == TITLE_LOCATION::USB) return dirExist(Paths::UsbUsrDir);
    //if (location == TITLE_LOCATION::Disc) return dirExist("storage_odd01:/usr/");
    return false;
}

bool isDiscInserted() {
    return false;
    // if (getCFWVersion() == TIRAMISU_RPX) {
    //     // Get the disc key via Tiramisu's CFW
    //     std::array<uint8_t, 16> discKey = {0};
    //     discKey.fill(0);
    //     int32_t result = IOSUHAX_ODM_GetDiscKey(discKey.data());
    //     if (result == 0) {
    //         // WHBLogPrintf("%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X", discKey.at(0), discKey.at(1), discKey.at(2), discKey.at(3), discKey.at(4), discKey.at(5), discKey.at(6), discKey.at(7), discKey.at(8), discKey.at(9), discKey.at(10), discKey.at(11), discKey.at(12), discKey.at(13), discKey.at(14), discKey.at(15));
    //         // WHBLogFreetypeDraw();
    //         // sleep_for(3s);
    //         return !(std::all_of(discKey.begin(), discKey.end(), [](uint8_t i) {return i==0;}));
    //     }
    //     else return false;
    // }
    // else {
    //     // Use the older method of detecting discs
    //     // The ios_odm module writes the disc key whenever a disc is inserted
    //     DCInvalidateRange((void*)0xF5E10C00, 32);
    //     return *(volatile uint32_t*)0xF5E10C00 != 0;
    // }
}

// Filesystem Helper Functions

// Wii U libraries will give us paths that use /vol/storage_mlc01/file.txt, but libiosuhax uses the mounted drive paths like storage_mlc01:/file.txt (and wut uses fs:/vol/sys_mlc01/file.txt)
// Converts a Wii U device path to a posix path
std::string convertToWiiUFsPath(std::string_view volPath) {
    std::string posixPath;

    // volPath has to start with /vol/
    if (!volPath.starts_with("/vol/")) return "";

    if (USE_LIBMOCHA()) {
        // Handle /vol/system/ as an alias for /vol/storage_slc/sys/
        if (volPath.substr(5, 6) == "system" && (volPath.size() == 11 || volPath[11] == '/')) {
            std::string path = "storage_slc:/sys";
            if (volPath.size() > 11) path += volPath.substr(11);
            else path += "/";
            return path;
        }

        // For the SD card, we want to use the fs: prefix since it's not mounted via mocha
        if (volPath.substr(5, 10) == "external01") {
            std::string path = "fs:";
            path += volPath;
            return path;
        }

        // Get and append the mount path
        size_t drivePathEnd = volPath.find('/', 5);
        if (drivePathEnd == std::string_view::npos) {
            // Return just the mount path
            posixPath.append(volPath.substr(5));
            posixPath.append(":");
        } else {
            // Return mount path + the path after it
            posixPath.append(volPath.substr(5, drivePathEnd - 5));
            posixPath.append(":/");
            posixPath.append(volPath.substr(drivePathEnd + 1));
        }
        return posixPath;
    }
    else {
        std::string path = "fs:";
        path += volPath;
        return path;
    }
}


struct stat existStat;
const std::regex rootFilesystem(R"(^fs:\/vol\/[^\/:]+\/?$)");
bool isRoot(std::string_view path) {
    if (path.size() >= 2 && path.back() == ':') return true;
    if (path.size() >= 3 && path[path.size() - 2] == ':' && path.back() == '/') return true;
    if (true/*!IS_CEMU_PRESENT()*/) {
        if (std::regex_match(path.begin(), path.end(), rootFilesystem)) return true;
    }
    return false;
}

bool fileExist(const std::string& path) {
    std::string convertedPath = convertToWiiUFsPath(path);
    if (isRoot(convertedPath)) return true;
    if (lstat(convertedPath.c_str(), &existStat) == 0 && S_ISREG(existStat.st_mode)) return true;
    return false;
}

bool dirExist(const std::string& path) {
    std::string convertedPath = convertToWiiUFsPath(path);
    if (isRoot(convertedPath)) return true;
    if (lstat(convertedPath.c_str(), &existStat) == 0 && S_ISDIR(existStat.st_mode)) return true;
    return false;
}

bool copyFile(const std::string& src, const std::string& dest) {
    std::string convertedSrc = convertToWiiUFsPath(src);
    std::string convertedDest = convertToWiiUFsPath(dest);
    try {
        return std::filesystem::copy_file(convertedSrc, convertedDest, std::filesystem::copy_options::overwrite_existing);
    } catch (...) {
        return false;
    }
}

bool moveFile(const std::string& src, const std::string& dest) {
    std::string convertedSrc = convertToWiiUFsPath(src);
    std::string convertedDest = convertToWiiUFsPath(dest);
    if (rename(convertedSrc.c_str(), convertedDest.c_str()) == 0) return true;
    if (copyFile(src, dest)) {
        removeFile(src);
        return true;
    }
    return false;
}

bool removeFile(const std::string& path) {
    std::string convertedPath = convertToWiiUFsPath(path);
    return unlink(convertedPath.c_str()) == 0;
}

bool removeDir(const std::string& path) {
    std::string convertedPath = convertToWiiUFsPath(path);
    return rmdir(convertedPath.c_str()) == 0;
}

bool createDirectories(const std::string& path) {
    std::string convertedPath = convertToWiiUFsPath(path);
    try {
        return std::filesystem::create_directories(convertedPath);
    } catch (...) {
        return false;
    }
}

int fileOpen(const std::string& path, int flags, mode_t mode) {
    std::string convertedPath = convertToWiiUFsPath(path);
    return open(convertedPath.c_str(), flags, mode);
}

FILE* fileFopen(const std::string& path, const char* mode) {
    std::string convertedPath = convertToWiiUFsPath(path);
    return fopen(convertedPath.c_str(), mode);
}

DIR* dirOpen(const std::string& path) {
    std::string convertedPath = convertToWiiUFsPath(path);
    return opendir(convertedPath.c_str());
}

bool isSlcPath(const std::string& path) {
    return path.rfind(Paths::SlcRoot, 0) == 0 || path.rfind(Paths::SystemRoot, 0) == 0;
}

uint64_t getFreeSpace(const std::string& path) {
    std::string convertedPath = convertToWiiUFsPath(path);
    struct statvfs s;
    if (statvfs(convertedPath.c_str(), &s) == 0) {
        return (uint64_t)s.f_bavail * s.f_frsize;
    }
    return 0;
}

uint64_t getFileSize(const std::string& path) {
    std::string convertedPath = convertToWiiUFsPath(path);
    struct stat s;
    if (lstat(convertedPath.c_str(), &s) == 0) {
        return s.st_size;
    }
    return 0;
}

size_t countFiles(const std::string& path) {
    DIR* dir = dirOpen(path);
    if (!dir) return 0;
    size_t count = 0;
    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_type == DT_REG) {
            count++;
        }
    }
    closedir(dir);
    return count;
}

bool deleteDirContent(const std::string& path) {
    DIR* dirHandle;
    if ((dirHandle = dirOpen(path.c_str())) == nullptr) return false;

    struct dirent *dirEntry;
    while((dirEntry = readdir(dirHandle)) != nullptr) {
        if (strcmp(dirEntry->d_name, ".") == 0 || strcmp(dirEntry->d_name, "..") == 0) continue;

        std::string fullPath = path;
        if (fullPath.back() != '/') fullPath += "/";
        fullPath += dirEntry->d_name;

        if ((dirEntry->d_type & DT_DIR) == DT_DIR) {
            deleteDirContent(fullPath);
            removeDir(fullPath);
        } else {
            removeFile(fullPath);
        }
    }

    closedir(dirHandle);
    return true;
}

bool isDirEmpty(const std::string& path) {
    DIR* dirHandle;
    if ((dirHandle = dirOpen(path.c_str())) == nullptr) return true;

    struct dirent *dirEntry;
    while((dirEntry = readdir(dirHandle)) != nullptr) {
        if ((dirEntry->d_type & DT_DIR) == DT_DIR && (strcmp(dirEntry->d_name, ".") == 0 || strcmp(dirEntry->d_name, "..") == 0)) continue;
        
        // An entry other than the root and parent directory was found
        closedir(dirHandle);
        return false;
    }
    
    closedir(dirHandle);
    return true;
}
