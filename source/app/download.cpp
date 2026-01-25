#include "download.h"
#include "gui.h"
#include "filesystem.h"
#include <curl/curl.h>
#include <wut_fsa.h>
#include <string>
#include <vector>

// Struct to hold FSA handles for the curl callback
struct FsaWriteData {
    int32_t clientHandle;
    int32_t fileHandle;
};

static size_t write_data_fsa(void *ptr, size_t size, size_t nmemb, void *stream) {
    FsaWriteData *data = (FsaWriteData *)stream;
    size_t bytes_to_write = size * nmemb;
    uint32_t bytes_written = 0;

    if (data->fileHandle >= 0) {
        FSAWriteFile(data->clientHandle, ptr, 1, bytes_to_write, data->fileHandle, 0, &bytes_written);
    }
    return bytes_written;
}

static void downloadFile(int32_t clientHandle, const std::string& url, const std::string& path) {
    WHBLogFreetypePrintf(L"Downloading %S...", toWstring(url).c_str());
    WHBLogFreetypeDrawScreen();

    CURL *curl_handle = curl_easy_init();
    if (!curl_handle) {
        WHBLogFreetypePrintf(L"Failed to initialize curl!");
        WHBLogFreetypeDrawScreen();
        sleep_for(3s);
        return;
    }

    int32_t fileHandle = -1;
    FSStatus status = FSAOpenFile(clientHandle, path.c_str(), "w", &fileHandle);
    if (status != FS_STATUS_OK) {
        WHBLogFreetypePrintf(L"Failed to open %S for writing! Error: %d", toWstring(path).c_str(), status);
        WHBLogFreetypeDrawScreen();
        sleep_for(3s);
        curl_easy_cleanup(curl_handle);
        return;
    }

    FsaWriteData fsa_data = {clientHandle, fileHandle};

    curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data_fsa);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &fsa_data);
    curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "Dumpling/2.0");
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);


    CURLcode res = curl_easy_perform(curl_handle);
    if (res != CURLE_OK) {
        WHBLogFreetypePrintf(L"Curl failed: %S", toWstring(curl_easy_strerror(res)).c_str());
        WHBLogFreetypeDrawScreen();
    } else {
        WHBLogFreetypePrintf(L"Successfully downloaded %S", toWstring(url).c_str());
        WHBLogFreetypeDrawScreen();
    }

    FSACloseFile(clientHandle, fileHandle);
    curl_easy_cleanup(curl_handle);
    sleep_for(1s);
}

void downloadHaxFiles() {
    WHBLogFreetypeStartScreen();
    WHBLogFreetypePrint(L"Starting download of hax files...");
    WHBLogFreetypeDrawScreen();
    sleep_for(2s);

    int32_t fsaHandle = FSAInit();
    if (fsaHandle < 0) {
        WHBLogFreetypePrintf(L"FSAInit failed: %d", fsaHandle);
        WHBLogFreetypeDrawScreen();
        sleep_for(3s);
        return;
    }

    int32_t clientHandle = -1;
    FSStatus mountStatus = FSAMount(fsaHandle, "/dev/sdcard01", "/vol/sdcard", FS_MOUNT_FLAG_WRITE, nullptr, 0, &clientHandle);
    if (mountStatus != FS_STATUS_OK) {
        WHBLogFreetypePrintf(L"Failed to mount SD card. Error: %d", mountStatus);
        WHBLogFreetypeDrawScreen();
        sleep_for(3s);
        FSAShutdown(fsaHandle);
        return;
    }

    std::vector<std::string> dirs = {"/vol/sdcard/system", "/vol/sdcard/system/hax", "/vol/sdcard/system/hax/installer"};
    for(const auto& dir : dirs) {
        FSStatus dirStatus = FSAMakeDir(clientHandle, dir.c_str(), 0);
        if (dirStatus != FS_STATUS_OK && dirStatus != FS_STATUS_EXISTS) {
            WHBLogFreetypePrintf(L"Failed to create directory %S. Error: %d", toWstring(dir).c_str(), dirStatus);
            WHBLogFreetypeDrawScreen();
            sleep_for(3s);
            FSAUnmount(clientHandle, "/vol/sdcard", 2);
            FSAShutdown(fsaHandle);
            return;
        }
    }

    downloadFile(clientHandle, "https://github.com/isfshax/isfshax_installer/releases/latest/download/ios.img", "/vol/sdcard/system/hax/installer/fw.img");
    downloadFile(clientHandle, "https://github.com/isfshax/isfshax/releases/latest/download/superblock.img", "/vol/sdcard/system/hax/installer/superblock.img");
    downloadFile(clientHandle, "https://github.com/isfshax/isfshax/releases/latest/download/superblock.img.sha", "/vol/sdcard/system/hax/installer/superblock.img.sha");

    FSAUnmount(clientHandle, "/vol/sdcard", 2);
    FSAShutdown(fsaHandle);

    WHBLogFreetypeClear();
    WHBLogFreetypePrint(L"All hax files downloaded successfully!");
    WHBLogFreetypeDrawScreen();
    sleep_for(3s);
}
