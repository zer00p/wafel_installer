#include "download.h"
#include "minute_config.h"
#include "gui.h"
#include "menu.h"
#include "cfw.h"
#include "filesystem.h"
#include "common_paths.h"
#include "common.h"
#include <curl/curl.h>
#include <string>
#include <cstring>
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
#include <random>
#include <algorithm> // Required for std::generate_n

/**
 * Inspired by AromaUpdater's download and extraction logic by Maschell.
 * https://github.com/wiiu-env/AromaUpdater
 */

namespace fs = std::filesystem;

// Helper function to generate a random alphanumeric string of a given length
static std::string generateRandomString(size_t length) {
    auto randchar = []() -> char {
        const char charset[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
        const size_t max_index = (sizeof(charset) - 1);
        std::random_device rd;
        std::mt19937 generator(rd());
        std::uniform_int_distribution<size_t> distribution(0, max_index);
        return charset[distribution(generator)];
    };
    std::string str(length, 0);
    std::generate_n(str.begin(), length, randchar);
    return str;
}

std::string getTempDownloadPath(const std::string& finalPath) {
    std::string tempDir;

    if (isSlcPath(finalPath)) {
        tempDir = "/vol/system/tmp/";
    } else {
        tempDir = Paths::SdRoot + "/tmp/"; // SD card temporary directory
    }

    createDirectories(tempDir); // Ensure temporary directory exists

    // "dl_" + 9 random chars = 12 chars, max 12 chars allowed on SLC;
    return tempDir + "dl_" + generateRandomString(9); 
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream) {
    int fd = *(int*)stream;
    ssize_t written = write(fd, ptr, size * nmemb);
    if (written == -1) {
        return 0; // Signal error to curl
    }
    return written;
}

bool downloadFile(const std::string& url, const std::string& path) {
    while (true) {
        WHBLogFreetypePrintf(L"Downloading %S to %S...", toWstring(url).c_str(), toWstring(path).c_str());
        WHBLogFreetypeDrawScreen();

        if (isSlcPath(path)) {
            uint64_t freeSpace = getFreeSpace(path);
            if (freeSpace < 30 * 1024 * 1024L) {
                setErrorPrompt(L"Download aborted: Less than 30MB free space on SLC!");
                if (showErrorPrompt(L"Cancel", true)) continue;
                return false;
            }
        }

        std::string tempPath = getTempDownloadPath(path);

        CURL *curl_handle = curl_easy_init();
        if (!curl_handle) {
            WHBLogFreetypePrintf(L"Failed to initialize curl!");
            WHBLogFreetypeDrawScreen();
            setErrorPrompt(L"Failed to initialize curl!");
            if (showErrorPrompt(L"Cancel", true)) {
                removeFile(tempPath); // Clean up temp file
                continue;
            }
            return false;
        }

        int fd = fileOpen(tempPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            WHBLogFreetypePrintf(L"Failed to open %S for writing!\nErrno: %d", toWstring(tempPath).c_str(), errno);
            WHBLogFreetypeDrawScreen();
            std::wstring error = L"Failed to open " + toWstring(tempPath) + L" for writing!\nErrno: " + std::to_wstring(errno);
            setErrorPrompt(error);
            curl_easy_cleanup(curl_handle);
            if (showErrorPrompt(L"Cancel", true)) {
                removeFile(tempPath); // Clean up temp file
                continue;
            }
            return false;
        }

        curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &fd);
        if (isSlcPath(path)) {
            curl_easy_setopt(curl_handle, CURLOPT_MAXFILESIZE, 10 * 1024 * 1024L); // 10MB limit for SLC
        }
        curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "wafel_installer/1.0");
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
            } else if (res == CURLE_FILESIZE_EXCEEDED) {
                error = L"Download aborted: File size exceeded 10MB limit for SLC!";
            }
            setErrorPrompt(error);
            if (showErrorPrompt(L"Cancel", true)) {
                removeFile(tempPath); // Clean up temp file
                continue;
            }
            removeFile(tempPath); // Clean up temp file on other errors
            return false;
        }

        // If download was successful, move the temporary file to the final path
        if (!moveFile(tempPath, path)) {
            WHBLogFreetypePrintf(L"Failed to move downloaded file from %S to %S", toWstring(tempPath).c_str(), toWstring(path).c_str());
            WHBLogFreetypeDrawScreen();
            setErrorPrompt(L"Failed to move downloaded file!");
            if (showErrorPrompt(L"Cancel", true)) {
                removeFile(tempPath); // Clean up temp file
                continue;
            }
            removeFile(tempPath); // Clean up temp file on error
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
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "wafel_installer/1.0");
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
    std::string url = "https://raw.githubusercontent.com/zer00p/wafel_installer/refs/heads/master/plugins.csv";

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

std::string getPluginUrl(const std::string& fileName) {
    fetchPluginList();
    for (const auto& p : getCachedPluginList()) {
        if (p.fileName == fileName) {
            return p.downloadPath;
        }
    }
    return "";
}

static bool createIsfsHaxDirectories() {
    if (!checkSystemAccess()) {
        return false;
    }
    createDirectories(Paths::SlcInstallerDir);
    return true;
}

static bool downloadBasePlugins() {
    std::string pluginPath = getStroopwafelPluginPath();
    createDirectories(pluginPath);
    bool res =  downloadFile(getPluginUrl("00core.ipx"),   pluginPath + "/00core.ipx") &&
                downloadFile(getPluginUrl("5isfshax.ipx"), pluginPath + "/5isfshax.ipx");
    bool hasAroma = dirExist(Paths::SdAromaDir);
    if(res && hasAroma)
        return downloadFile(getPluginUrl("5payldr.ipx"),  pluginPath + "/5payldr.ipx");
    return res;
}

bool downloadStroopwafelFiles(bool toSD) {
    std::string plugin_dir = toSD? Paths::SdPluginsDir : Paths::SlcPluginsDir;
    setStroopwafelPluginPath(plugin_dir);

    if (toSD) {
        if (WHBMountSdCard() != 1) {
            setErrorPrompt(L"Failed to mount SD card!");
            return false;
        }
        if (!downloadBasePlugins() ||
            !downloadFile("https://github.com/StroopwafelCFW/minute_minute/releases/latest/download/fw.img", Paths::SdFwImg))
            return false;

        ensureMinuteIni();


    } else {
        if (!downloadBasePlugins() ||
            !downloadFile("https://github.com/StroopwafelCFW/minute_minute/releases/latest/download/fw_fastboot.img", Paths::SlcFwImg))
            return false;
    }
    setStroopwafelDownloadedInSession(true);
    return true;
}

bool downloadIsfshaxFiles() {
    if (!createIsfsHaxDirectories()) return false;

    std::string slcFwImgPath = Paths::SlcInstallerFwImg;

    if (!downloadFile("https://github.com/isfshax/isfshax/releases/latest/download/superblock.img", Paths::SlcInstallerSblockImg) ||
        !downloadFile("https://github.com/isfshax/isfshax/releases/latest/download/superblock.img.sha", Paths::SlcInstallerSblockSha) ||
        !downloadFile("https://github.com/isfshax/isfshax_installer/releases/latest/download/ios.img", slcFwImgPath))
    {
        return false;
    }

    // Also copy installer to SD root as ios.img if real SD exists
    if (!isSdEmulated() && WHBMountSdCard() == 1) {
        WHBLogFreetypePrintf(L"Copying installer to SD...");
        WHBLogFreetypeDrawScreen();
        copyFile(slcFwImgPath, Paths::SdRoot + "/ios.img");
    }

    return true;
}

static void removeCompetingPlugins() {
    std::string path = getStroopwafelPluginPath();
    if (path.empty()) return;
    if (path.back() != '/') path += "/";
    removeFile(path + "5usbpart.ipx");
    removeFile(path + "5upartsd.ipx");
}

bool downloadPlugin(std::string pluginFile) {
    std::string path = getStroopwafelPluginPath();
    if (path.back() != '/') path += "/";
    path += pluginFile;
    return downloadFile(getPluginUrl(pluginFile), path);
}

bool downloadUsbPartitionPlugin(bool sdEmulation) {
    std::string pluginFile = sdEmulation ? "5upartsd.ipx" : "5usbpart.ipx";
    removeCompetingPlugins();
    return downloadPlugin(pluginFile);
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

std::string getLatestReleaseAssetDigest(const std::string& repo, const std::string& assetName) {
    std::string apiResponse;
    if (!downloadToBuffer("https://api.github.com/repos/" + repo + "/releases/latest", apiResponse)) {
        return "";
    }
    return getDigestFromResponse(apiResponse, assetName);
}

std::string getDigestFromResponse(const std::string& apiResponse, const std::string& pattern) {
    size_t pos = 0;
    while ((pos = apiResponse.find("\"name\":", pos)) != std::string::npos) {
        pos += 7;
        size_t nameStart = apiResponse.find("\"", pos);
        if (nameStart == std::string::npos) break;
        nameStart++;
        size_t nameEnd = apiResponse.find("\"", nameStart);
        if (nameEnd == std::string::npos) break;
        std::string assetName = apiResponse.substr(nameStart, nameEnd - nameStart);

        bool match = false;
        if (pattern.find(".") != std::string::npos) {
            match = (assetName == pattern);
        } else {
            match = (assetName.find(pattern) != std::string::npos);
        }

        if (match) {
            size_t digestPos = apiResponse.find("\"digest\":", nameEnd);
            size_t nextNamePos = apiResponse.find("\"name\":", nameEnd);
            if (digestPos != std::string::npos && (nextNamePos == std::string::npos || digestPos < nextNamePos)) {
                size_t endOfDigest = apiResponse.find_first_of(",}", digestPos);
                std::string digestVal = apiResponse.substr(digestPos + 9, endOfDigest - (digestPos + 9));

                size_t valStart = digestVal.find("\"sha256:");
                if (valStart != std::string::npos) {
                    valStart += 8;
                    size_t valEnd = digestVal.find("\"", valStart);
                    if (valEnd != std::string::npos) {
                        return digestVal.substr(valStart, valEnd - valStart);
                    }
                }
            }
            return "";
        }
        pos = nameEnd;
    }

    return "";
}

std::string getRepoFromUrl(const std::string& url) {
    std::string search = "github.com/repos/";
    size_t pos = url.find(search);
    if (pos == std::string::npos) {
        search = "github.com/";
        pos = url.find(search);
        if (pos == std::string::npos) return "";
    }
    pos += search.length();
    size_t end = url.find("/releases/", pos);
    if (end == std::string::npos) return "";
    return url.substr(pos, end - pos);
}

static bool downloadAndExtractZip(const std::string& repo, const std::string& pattern, const std::string& displayName, const std::string& path, std::function<std::string(std::string)> pathMapper = nullptr) {
    std::string zipUrl = getLatestReleaseAssetUrl(repo, pattern);
    if (zipUrl.empty()) return false;

    std::string zipData;
    if (!downloadToBuffer(zipUrl, zipData)) return false;

    WHBLogFreetypePrintf(L"Extracting %S...", toWstring(displayName).c_str());
    WHBLogFreetypeDrawScreen();

    if (isSlcPath(path)) {
        uint64_t freeSpace = getFreeSpace(path);
        if (freeSpace < 30 * 1024 * 1024L) {
            setErrorPrompt(L"Extraction aborted: Less than 30MB free space on SLC!");
            return false;
        }
    }

    try {
        std::istringstream iss(zipData);
        miniz_cpp::zip_file zip(iss);

        for (auto& info : zip.infolist()) {
            std::string targetFilename = info.filename;
            if (pathMapper) {
                targetFilename = pathMapper(info.filename);
                if (targetFilename.empty()) continue; // Skip if mapped to empty
            }

            std::string fullPath = path + "/" + targetFilename;
            if (targetFilename.back() == '/') {
                createDirectories(fullPath);
            } else {
                fs::path p(fullPath);
                createDirectories(p.parent_path());

                std::string data = zip.read(info);
                int fd = fileOpen(fullPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
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

bool downloadAroma() {
    WHBLogFreetypeStartScreen();

    std::string targetPath = Paths::SdRoot;
    if (WHBMountSdCard() != 1) {
        setErrorPrompt(L"Failed to mount SD card!");
        return false;
    }

    // 1. Environment Loader
    if (!downloadAndExtractZip("wiiu-env/EnvironmentLoader", "EnvironmentLoader", "Environment Loader", targetPath)) {
        return false;
    }

    // 2. Custom RPX Loader (remapped)
    auto customRpxMapper = [](std::string path) -> std::string {
        if (path == "wiiu/payload.elf") return "wiiu/payloads/default/payload.elf";
        return path;
    };
    if (!downloadAndExtractZip("wiiu-env/CustomRPXLoader", "CustomRPXLoader", "Custom RPX Loader", targetPath, customRpxMapper)) {
        return false;
    }

    // 3. Payload Loader Payload
    if (!downloadAndExtractZip("wiiu-env/PayloadLoaderPayload", "PayloadLoaderPayload", "Payload Loader Payload", targetPath)) {
        return false;
    }

    // 4. Aroma
    if (!downloadAndExtractZip("wiiu-env/Aroma", "aroma", "Aroma", targetPath)) {
        return false;
    }

    // 5. HB App Store
    std::string appstoreUrl = getLatestReleaseAssetUrl("fortheusers/hb-appstore", "appstore.wuhb");
    if (appstoreUrl.empty()) return false;

    std::string appstorePath = targetPath + "/wiiu/apps/appstore/";
    createDirectories(appstorePath);
    if (!downloadFile(appstoreUrl, appstorePath + "appstore.wuhb")) {
        return false;
    }

    WHBLogFreetypePrint(L"Aroma and tools installed successfully!");
    WHBLogFreetypeDrawScreen();

    // Freshly downloaded Aroma, also download payloader plugin if Stroopwafel is present
    std::string pluginPath = getStroopwafelPluginPath();
    if (!pluginPath.empty() && dirExist(pluginPath)) {
        std::string target = pluginPath;        
        if (target.back() != '/') target += "/";
        target += "5payldr.ipx";

        if (pluginPath.find("storage_slc") != std::string::npos) {
            if (checkSystemAccess()) {
                downloadFile(getPluginUrl("5payldr.ipx"), target);
            }
        } else {
            downloadFile(getPluginUrl("5payldr.ipx"), target);
        }
    }

    return true;
}
