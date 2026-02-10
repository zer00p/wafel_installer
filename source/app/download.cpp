#include "download.h"
#include "gui.h"
#include "menu.h"
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
#include <whb/sdcard.h>
#include "cacert_pem.h"
#include "../utils/zip_file.hpp"
#include <filesystem>
#include <sstream>
#include <functional>

namespace fs = std::filesystem;

static size_t write_data_posix(void *ptr, size_t size, size_t nmemb, void *stream) {
    int fd = *(int*)stream;
    ssize_t written = write(fd, ptr, size * nmemb);
    if (written == -1) {
        return 0; // Signal error to curl
    }
    return written;
}

bool downloadFile(const std::string& url, const std::string& path) {
    while (true) {
        WHBLogFreetypePrintf(L"Downloading %S...", toWstring(url).c_str());
        WHBLogFreetypeDrawScreen();

        CURL *curl_handle = curl_easy_init();
        if (!curl_handle) {
            WHBLogFreetypePrintf(L"Failed to initialize curl!");
            WHBLogFreetypeDrawScreen();
            setErrorPrompt(L"Failed to initialize curl!");
            if (showErrorPrompt(L"Cancel", true)) continue;
            return false;
        }

        int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            WHBLogFreetypePrintf(L"Failed to open %S for writing! Errno: %d", toWstring(path).c_str(), errno);
            WHBLogFreetypeDrawScreen();
            std::wstring error = L"Failed to open " + toWstring(path) + L" for writing! Errno: " + std::to_wstring(errno);
            setErrorPrompt(error);
            curl_easy_cleanup(curl_handle);
            if (showErrorPrompt(L"Cancel", true)) continue;
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
            std::wstring error = L"Curl failed: " + toWstring(curl_easy_strerror(res));
            if (res == CURLE_PEER_FAILED_VERIFICATION || res == CURLE_SSL_CONNECT_ERROR) {
                error += L"\nPlease check if your system date and time are correct!";
            }
            setErrorPrompt(error);
            if (showErrorPrompt(L"Cancel", true)) continue;
            return false;
        }

        WHBLogFreetypePrintf(L"Successfully downloaded %S", toWstring(url).c_str());
        WHBLogFreetypeDrawScreen();
        return true;
    }
}

static size_t write_data_buffer(void *ptr, size_t size, size_t nmemb, void *stream) {
    std::string* buffer = (std::string*)stream;
    buffer->append((char*)ptr, size * nmemb);
    return size * nmemb;
}

bool downloadToBuffer(const std::string& url, std::string& buffer) {
    while (true) {
        WHBLogFreetypePrintf(L"Downloading %S...", toWstring(url).c_str());
        WHBLogFreetypeDrawScreen();

        CURL *curl_handle = curl_easy_init();
        if (!curl_handle) {
            setErrorPrompt(L"Failed to initialize curl!");
            if (showErrorPrompt(L"Cancel", true)) continue;
            return false;
        }

        curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data_buffer);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &buffer);
        curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "ISFShaxLoader/1.0");
        curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);

        curl_blob blob;
        blob.data  = (void *) cacert_pem;
        blob.len   = cacert_pem_size;
        blob.flags = CURL_BLOB_COPY;
        curl_easy_setopt(curl_handle, CURLOPT_CAINFO_BLOB, &blob);

        CURLcode res = curl_easy_perform(curl_handle);
        curl_easy_cleanup(curl_handle);

        if (res != CURLE_OK) {
            setErrorPrompt(L"Curl failed for " + toWstring(url) + L":\n" + toWstring(curl_easy_strerror(res)));
            if (showErrorPrompt(L"Cancel", true)) {
                buffer.clear();
                continue;
            }
            return false;
        }

        return true;
    }
}

static std::vector<Plugin> cachedPluginList;
static bool failedToFetch = false;

const std::vector<Plugin>& getCachedPluginList() {
    return cachedPluginList;
}

bool fetchPluginList(bool force) {
    if (!cachedPluginList.empty() && !force) return true;
    if (failedToFetch && !force) return false;

    std::string csvData;
    std::string url = "https://raw.githubusercontent.com/zer00p/isfshax-loader/refs/heads/master/plugins.csv";

    if (!downloadToBuffer(url, csvData)) {
        failedToFetch = true;
        return false;
    }
    failedToFetch = false;

    std::stringstream ss(csvData);
    std::string line;
    // skip header
    if (!std::getline(ss, line)) return false;

    cachedPluginList.clear();
    while (std::getline(ss, line)) {
        if (line.empty() || line == "\r") continue;
        if (line.back() == '\r') line.pop_back(); // Handle Windows line endings

        std::stringstream lineStream(line);
        std::string cell;
        std::vector<std::string> cells;
        while (std::getline(lineStream, cell, ';')) {
            cells.push_back(cell);
        }
        // Handle case where line ends with a semicolon
        if (!line.empty() && line.back() == ';') {
            cells.push_back("");
        }

        if (cells.size() >= 4) {
            Plugin p;
            p.shortDescription = cells[0];
            p.fileName = cells[1];
            p.downloadPath = cells[2];
            p.longDescription = cells[3];
            if (cells.size() >= 5) p.incompatiblePlugins = cells[4];
            cachedPluginList.push_back(p);
        }
    }

    return !cachedPluginList.empty();
}

static std::string getPluginUrl(const std::string& fileName) {
    fetchPluginList();
    for (const auto& p : getCachedPluginList()) {
        if (p.fileName == fileName) {
            return p.downloadPath;
        }
    }
    return "";
}

static bool createHaxDirectories() {
    if (!isSlcMounted()) {
        WHBLogFreetypePrintf(L"Failed to mount SLC! FTP system file access enabled?");
        WHBLogFreetypeDrawScreen();
        setErrorPrompt(L"Failed to mount SLC! FTP system file access enabled?");
        return false;
    }

    std::vector<std::string> dirs = {"/vol/storage_slc/sys/hax", "/vol/storage_slc/sys/hax/installer", "/vol/storage_slc/sys/hax/ios_plugins"};
    for(const auto& dir : dirs) {
        std::string posix_path = convertToPosixPath(dir.c_str());
        WHBLogFreetypePrintf(L"Create directory %S.", toWstring(posix_path).c_str());
        if (mkdir(posix_path.c_str(), 0755) != 0 && errno != EEXIST) {
            WHBLogFreetypePrintf(L"Failed to create directory %S. Errno: %d", toWstring(posix_path).c_str(), errno);
            WHBLogFreetypeDrawScreen();
            std::wstring error = L"Failed to create directory " + toWstring(posix_path) + L". Errno: " + std::to_wstring(errno);
            setErrorPrompt(error);
            return false;
        }
    }
    return true;
}

bool downloadStroopwafelFiles(bool toSD) {
    if (toSD) {
        if (WHBMountSdCard() != 1) {
            setErrorPrompt(L"Failed to mount SD card!");
            return false;
        }
        std::string sdPluginPath = "fs:/vol/external01/wiiu/ios_plugins/";
        fs::create_directories(sdPluginPath);

        if (!downloadFile(getPluginUrl("00core.ipx"), sdPluginPath + "00core.ipx") ||
            !downloadFile(getPluginUrl("5isfshax.ipx"), sdPluginPath + "5isfshax.ipx") ||
            !downloadFile(getPluginUrl("5payldr.ipx"), sdPluginPath + "5payldr.ipx") ||
            !downloadFile("https://github.com/StroopwafelCFW/minute_minute/releases/latest/download/fw_fastboot.img", "fs:/vol/external01/fw.img"))
        {
            return false;
        }
    } else {
        if (!createHaxDirectories()) return false;
        if (!downloadFile(getPluginUrl("00core.ipx"), convertToPosixPath("/vol/storage_slc/sys/hax/ios_plugins/00core.ipx")) ||
            !downloadFile(getPluginUrl("5isfshax.ipx"), convertToPosixPath("/vol/storage_slc/sys/hax/ios_plugins/5isfshax.ipx")) ||
            !downloadFile(getPluginUrl("5payldr.ipx"), convertToPosixPath("/vol/storage_slc/sys/hax/ios_plugins/5payldr.ipx")) ||
            !downloadFile("https://github.com/StroopwafelCFW/minute_minute/releases/latest/download/fw_fastboot.img", convertToPosixPath("/vol/storage_slc/sys/hax/fw.img")))
        {
            return false;
        }
    }
    return true;
}

bool downloadIsfshaxFiles() {
    if (!createHaxDirectories()) return false;

    // Check for superblock.img on SD
    WHBMountSdCard();
    if (fileExist("fs:/vol/external01/superblock.img")) {
        if (showDialogPrompt(L"A superblock.img was found on the SD card.\nDo you want to remove it so the latest one gets used?", L"Yes", L"No") == 0) {
            remove("fs:/vol/external01/superblock.img");
        }
    }

    if (!downloadFile("https://github.com/isfshax/isfshax/releases/latest/download/superblock.img", convertToPosixPath("/vol/storage_slc/sys/hax/installer/sblock.img")) ||
        !downloadFile("https://github.com/isfshax/isfshax/releases/latest/download/superblock.img.sha", convertToPosixPath("/vol/storage_slc/sys/hax/installer/sblock.sha")) ||
        !downloadFile("https://github.com/isfshax/isfshax_installer/releases/latest/download/ios.img", convertToPosixPath("/vol/storage_slc/sys/hax/installer/fw.img")))
    {
        return false;
    }

    // Also download installer to SD root as ios.img if real SD exists
    if (WHBMountSdCard() == 1) {
        downloadFile("https://github.com/isfshax/isfshax_installer/releases/latest/download/ios.img", "fs:/vol/external01/ios.img");
    }

    return true;
}

bool downloadHaxFiles() {
    WHBLogFreetypeStartScreen();
    WHBLogFreetypePrint(L"Starting download of hax files...");
    WHBLogFreetypeDrawScreen();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    return downloadStroopwafelFiles(false) && downloadIsfshaxFiles();
}

bool downloadHaxFilesToSD() {
    return downloadStroopwafelFiles(true);
}

bool download5sdusb(bool toSLC, bool toSD) {
    bool success = true;
    if (toSLC) {
        if (!createHaxDirectories()) return false;
        success &= downloadFile(getPluginUrl("5sdusb.ipx"), convertToPosixPath("/vol/storage_slc/sys/hax/ios_plugins/5sdusb.ipx"));
    }
    if (toSD) {
        std::string sdPluginPath = "fs:/vol/external01/wiiu/ios_plugins/";
        fs::create_directories(sdPluginPath);
        success &= downloadFile(getPluginUrl("5sdusb.ipx"), sdPluginPath + "5sdusb.ipx");
    }
    return success;
}

bool download5upartsd(bool toSLC) {
    if (toSLC) {
        if (!createHaxDirectories()) return false;
        return downloadFile(getPluginUrl("5upartsd.ipx"), convertToPosixPath("/vol/storage_slc/sys/hax/ios_plugins/5upartsd.ipx"));
    }
    return true;
}


static std::string getLatestReleaseAssetUrl(const std::string& repo, const std::string& pattern) {
    std::string apiResponse;
    if (!downloadToBuffer("https://api.github.com/repos/" + repo + "/releases/latest", apiResponse)) {
        // downloadToBuffer already set the error prompt
        return "";
    }

    std::string searchKey = "\"browser_download_url\":";
    size_t pos = 0;
    while ((pos = apiResponse.find(searchKey, pos)) != std::string::npos) {
        pos += searchKey.length();
        size_t start = apiResponse.find("\"", pos);
        if (start == std::string::npos) break;
        start++;
        size_t end = apiResponse.find("\"", start);
        if (end == std::string::npos) break;
        std::string url = apiResponse.substr(start, end - start);
        // If pattern contains a dot, assume it's a full filename match, otherwise match pattern and .zip
        if (url.find(pattern) != std::string::npos && (pattern.find(".") != std::string::npos || url.find(".zip") != std::string::npos)) {
            return url;
        }
        pos = end;
    }

    setErrorPrompt(L"Failed to find asset matching '" + toWstring(pattern) + L"' in " + toWstring(repo));
    return "";
}

static bool downloadAndExtractZip(const std::string& repo, const std::string& pattern, const std::string& displayName, const std::string& sdPath, std::function<std::string(std::string)> pathMapper = nullptr) {
    std::string zipUrl = getLatestReleaseAssetUrl(repo, pattern);
    if (zipUrl.empty()) return false;

    std::string zipData;
    if (!downloadToBuffer(zipUrl, zipData)) return false;

    WHBLogFreetypePrintf(L"Extracting %S...", toWstring(displayName).c_str());
    WHBLogFreetypeDrawScreen();

    try {
        std::istringstream iss(zipData);
        miniz_cpp::zip_file zip(iss);

        for (auto& info : zip.infolist()) {
            std::string targetFilename = info.filename;
            if (pathMapper) {
                targetFilename = pathMapper(info.filename);
                if (targetFilename.empty()) continue; // Skip if mapped to empty
            }

            std::string fullPath = sdPath + targetFilename;
            if (targetFilename.back() == '/') {
                fs::create_directories(fullPath);
            } else {
                fs::path p(fullPath);
                fs::create_directories(p.parent_path());

                std::string data = zip.read(info);
                int fd = open(fullPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd >= 0) {
                    write(fd, data.data(), data.size());
                    close(fd);
                } else {
                    setErrorPrompt(L"Failed to open " + toWstring(fullPath) + L" for writing! Errno: " + std::to_wstring(errno));
                    return false;
                }
            }
        }
    } catch (const std::exception& e) {
        setErrorPrompt(toWstring(displayName) + L" extraction failed:\n" + toWstring(e.what()));
        return false;
    }

    return true;
}

bool downloadAroma(const std::string& sdPath) {
    WHBLogFreetypeStartScreen();

    if (sdPath == "fs:/vol/external01/") {
        WHBLogPrint("Mounting SD card...");
        WHBLogFreetypeDraw();
        if (WHBMountSdCard() != 1) {
            setErrorPrompt(L"Failed to mount SD card!");
            return false;
        }
    }

    // 1. Environment Loader
    if (!downloadAndExtractZip("wiiu-env/EnvironmentLoader", "EnvironmentLoader", "Environment Loader", sdPath)) {
        return false;
    }

    // 2. Custom RPX Loader (remapped)
    auto customRpxMapper = [](std::string path) -> std::string {
        if (path == "wiiu/payload.elf") return "wiiu/payloads/default/payload.elf";
        return path;
    };
    if (!downloadAndExtractZip("wiiu-env/CustomRPXLoader", "CustomRPXLoader", "Custom RPX Loader", sdPath, customRpxMapper)) {
        return false;
    }

    // 3. Payload Loader Payload
    if (!downloadAndExtractZip("wiiu-env/PayloadLoaderPayload", "PayloadLoaderPayload", "Payload Loader Payload", sdPath)) {
        return false;
    }

    // 4. Aroma
    if (!downloadAndExtractZip("wiiu-env/Aroma", "aroma", "Aroma", sdPath)) {
        return false;
    }

    // 5. HB App Store
    std::string appstoreUrl = getLatestReleaseAssetUrl("fortheusers/hb-appstore", "appstore.wuhb");
    if (appstoreUrl.empty()) return false;

    std::string appstorePath = sdPath + "wiiu/apps/appstore/";
    fs::create_directories(appstorePath);
    if (!downloadFile(appstoreUrl, appstorePath + "appstore.wuhb")) {
        return false;
    }

    WHBLogFreetypePrint(L"Aroma and tools installed successfully!");
    WHBLogFreetypeDrawScreen();
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
        return false;
    }

    return true;
}
