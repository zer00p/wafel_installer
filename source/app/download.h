#pragma once

#include <string>
#include <string_view>
#include <vector>

struct Plugin {
    std::string shortDescription;
    std::string fileName;
    std::string downloadPath;
    std::string longDescription;
    std::string incompatiblePlugins;
};

bool fetchPluginList(bool force = false);
const std::vector<Plugin>& getCachedPluginList();
std::string getPluginUrl(const std::string& fileName);

bool downloadHaxFiles();
bool downloadHaxFilesToSD();
bool downloadStroopwafelFiles(bool toSD);
bool downloadIsfshaxFiles();
bool downloadUsbPartitionPlugin(const std::string& pluginFile, const std::string& targetPosixPath);
bool downloadAroma(void);
bool downloadFile(const std::string& url, const std::string& path);
bool downloadToBuffer(const std::string& url, std::string& buffer);
std::string getLatestReleaseAssetDigest(const std::string& repo, const std::string& assetName);
std::string getDigestFromResponse(const std::string& apiResponse, const std::string& assetName);
std::string getRepoFromUrl(const std::string& url);
