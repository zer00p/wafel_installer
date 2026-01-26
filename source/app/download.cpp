#include "download.h"
#include "gui.h"
#include "filesystem.h"
#include "common.h"
#include <curl/curl.h>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <mocha/mocha.h>
#include "cacert_pem.h"

static size_t write_data_posix(void *ptr, size_t size, size_t nmemb, void *stream) {
    int fd = *(int*)stream;
    ssize_t written = write(fd, ptr, size * nmemb);
    if (written == -1) {
        return 0; // Signal error to curl
    }
    return written;
}

static bool downloadFile(const std::string& url, const std::string& path) {
    WHBLogFreetypePrintf(L"Downloading %S...", toWstring(url).c_str());
    WHBLogFreetypeDrawScreen();

    CURL *curl_handle = curl_easy_init();
    if (!curl_handle) {
        WHBLogFreetypePrintf(L"Failed to initialize curl!");
        WHBLogFreetypeDrawScreen();
        return false;
    }

    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        WHBLogFreetypePrintf(L"Failed to open %S for writing! Errno: %d", toWstring(path).c_str(), errno);
        WHBLogFreetypeDrawScreen();
        curl_easy_cleanup(curl_handle);
        return false;
    }

    curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data_posix);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &fd);
    curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "ISFShaxLoader/1.0");
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);

    // set cert
    curl_blob blob;
    blob.data  = (void *) cacert_pem;
    blob.len   = cacert_pem_size;
    blob.flags = CURL_BLOB_COPY;
    curl_easy_setopt(curl_handle, CURLOPT_CAINFO_BLOB, &blob);

    CURLcode res = curl_easy_perform(curl_handle);
    close(fd);
    curl_easy_cleanup(curl_handle);

    if (res != CURLE_OK) {
        WHBLogFreetypePrintf(L"Curl failed: %S", toWstring(curl_easy_strerror(res)).c_str());
        WHBLogFreetypeDrawScreen();
        return false;
    }

    WHBLogFreetypePrintf(L"Successfully downloaded %S", toWstring(url).c_str());
    WHBLogFreetypeDrawScreen();
    return true;
}

static bool createHaxDirectories() {
    if (!isSlcMounted()) {
        WHBLogFreetypePrintf(L"Failed to mount SLC! FTP system file access enabled?");
        WHBLogFreetypeDrawScreen();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return false;
    }

    std::vector<std::string> dirs = {"/vol/storage_slc/sys/hax", "/vol/storage_slc/sys/hax/installer", "/vol/storage_slc/sys/hax/ios_plugins"};
    for(const auto& dir : dirs) {
        std::string posix_path = convertToPosixPath(dir.c_str());
        WHBLogFreetypePrintf(L"Create directory %S.", toWstring(posix_path).c_str());
        if (mkdir(posix_path.c_str(), 0755) != 0 && errno != EEXIST) {
            WHBLogFreetypePrintf(L"Failed to create directory %S. Errno: %d", toWstring(posix_path).c_str(), errno);
            WHBLogFreetypeDrawScreen();
            std::this_thread::sleep_for(std::chrono::seconds(2));
            return false;
        }
    }
    return true;
}

bool downloadHaxFiles() {
    WHBLogFreetypeStartScreen();
    WHBLogFreetypePrint(L"Starting download of hax files...");
    WHBLogFreetypeDrawScreen();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    if (!createHaxDirectories()) return false;

    // Stroopwafel
    if (!downloadFile("https://github.com/StroopwafelCFW/stroopwafel/releases/latest/download/00core.ipx", convertToPosixPath("/vol/storage_slc/sys/hax/ios_plugins/00core.ipx")) ||
        !downloadFile("https://github.com/isfshax/wafel_isfshax_patch/releases/latest/download/5isfshax.ipx", convertToPosixPath("/vol/storage_slc/sys/hax/ios_plugins/5payldr.ipx")) ||
        !downloadFile("https://github.com/StroopwafelCFW/wafel_usb_partition/releases/latest/download/5upartsd.ipx", convertToPosixPath("/vol/storage_slc/sys/hax/ios_plugins/5upartsd.ipx")) ||
        !downloadFile("https://github.com/StroopwafelCFW/wafel_payloader/releases/latest/download/5payldr.ipx", convertToPosixPath("/vol/storage_slc/sys/hax/ios_plugins/5isfshax.ipx")) ||
        // minute
        !downloadFile("https://github.com/StroopwafelCFW/minute_minute/releases/latest/download/fw_fastboot.img", convertToPosixPath("/vol/storage_slc/sys/hax/fw.img")) ||
        // ISFShax
        !downloadFile("https://github.com/isfshax/isfshax/releases/latest/download/superblock.img", convertToPosixPath("/vol/storage_slc/sys/hax/installer/sblock.img")) ||
        !downloadFile("https://github.com/isfshax/isfshax/releases/latest/download/superblock.img.sha", convertToPosixPath("/vol/storage_slc/sys/hax/installer/sblock.sha")) ||
        !downloadFile("https://github.com/isfshax/isfshax_installer/releases/latest/download/ios.img", convertToPosixPath("/vol/storage_slc/sys/hax/installer/fw.img"))) 
    {
        WHBLogFreetypePrint(L"\nDownload failed. Please check your internet connection.");
        WHBLogFreetypeDrawScreen();
        return false;
    }

    return true;
}

bool downloadInstallerOnly() {
    WHBLogFreetypeStartScreen();
    WHBLogFreetypePrint(L"Starting download of installer...");
    WHBLogFreetypeDrawScreen();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    if (!createHaxDirectories()) return false;

    if (!downloadFile("https://github.com/isfshax/isfshax_installer/releases/latest/download/ios.img", convertToPosixPath("/vol/storage_slc/sys/hax/installer/fw.img")))
    {
        WHBLogFreetypePrint(L"\nDownload failed. Please check your internet connection.");
        WHBLogFreetypeDrawScreen();
        return false;
    }

    return true;
}
