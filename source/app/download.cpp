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

static void downloadFile(const std::string& url, const std::string& path) {
    WHBLogFreetypePrintf(L"Downloading %S...", toWstring(url).c_str());
    WHBLogFreetypeDrawScreen();

    CURL *curl_handle = curl_easy_init();
    if (!curl_handle) {
        WHBLogFreetypePrintf(L"Failed to initialize curl!");
        WHBLogFreetypeDrawScreen();
        std::this_thread::sleep_for(std::chrono::seconds(3));
        return;
    }

    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        WHBLogFreetypePrintf(L"Failed to open %S for writing! Errno: %d", toWstring(path).c_str(), errno);
        WHBLogFreetypeDrawScreen();
        std::this_thread::sleep_for(std::chrono::seconds(3));
        curl_easy_cleanup(curl_handle);
        return;
    }

    curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data_posix);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &fd);
    curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "Dumpling/2.0");
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);

    // set cert
    curl_blob blob;
    blob.data  = (void *) cacert_pem;
    blob.len   = cacert_pem_size;
    blob.flags = CURL_BLOB_COPY;
    curl_easy_setopt(curl_handle, CURLOPT_CAINFO_BLOB, &blob);

    CURLcode res = curl_easy_perform(curl_handle);
    if (res != CURLE_OK) {
        WHBLogFreetypePrintf(L"Curl failed: %S", toWstring(curl_easy_strerror(res)).c_str());
        WHBLogFreetypeDrawScreen();
    } else {
        WHBLogFreetypePrintf(L"Successfully downloaded %S", toWstring(url).c_str());
        WHBLogFreetypeDrawScreen();
    }

    close(fd);
    curl_easy_cleanup(curl_handle);
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

void downloadHaxFiles() {
    WHBLogFreetypeStartScreen();
    WHBLogFreetypePrint(L"Starting download of hax files...");
    WHBLogFreetypeDrawScreen();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    WHBLogFreetypePrint(L"Mounting SLC...");
    if (Mocha_MountFS("storage_slc", "/dev/slc01", "/vol/storage_slc01") != MOCHA_RESULT_SUCCESS) {
        WHBLogFreetypePrintf(L"Failed to mount SLC!");
        WHBLogFreetypeDrawScreen();
        std::this_thread::sleep_for(std::chrono::seconds(3));
        return;
    }

    std::vector<std::string> dirs = {"/vol/storage_slc/sys/hax", "/vol/storage_slc/sys/hax/installer"};
    for(const auto& dir : dirs) {
        std::string posix_path = convertToPosixPath(dir.c_str());
        WHBLogFreetypePrintf(L"Create directory %S.", toWstring(posix_path).c_str());
        if (mkdir(posix_path.c_str(), 0755) != 0 && errno != EEXIST) {
            WHBLogFreetypePrintf(L"Failed to create directory %S. Errno: %d", toWstring(posix_path).c_str(), errno);
            WHBLogFreetypeDrawScreen();
            std::this_thread::sleep_for(std::chrono::seconds(3));
            Mocha_UnmountFS("storage_slc");
            return;
        }
    }

    downloadFile("https://github.com/isfshax/isfshax/releases/latest/download/superblock.img", convertToPosixPath("/vol/storage_slc/sys/hax/installer/sblock.img"));
    downloadFile("https://github.com/isfshax/isfshax/releases/latest/download/superblock.img.sha", convertToPosixPath("/vol/storage_slc/sys/hax/installer/sblock.sha"));
    downloadFile("https://github.com/isfshax/isfshax_installer/releases/latest/download/ios.img", convertToPosixPath("/vol/storage_slc/sys/hax/installer/fw.img"));

    Mocha_UnmountFS("storage_slc");

    WHBLogFreetypeClear();
    WHBLogFreetypePrint(L"All hax files downloaded successfully!");
    WHBLogFreetypeDrawScreen();
    std::this_thread::sleep_for(std::chrono::seconds(3));
}
