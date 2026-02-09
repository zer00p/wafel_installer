#pragma once

#include <string>
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

bool downloadHaxFiles();
bool downloadHaxFilesToSD();
bool download5sdusb(bool toSLC, bool toSD);
bool download5upartsd(bool toSLC);
bool downloadInstallerOnly();
bool downloadAroma(const std::string& sdPath = "fs:/vol/external01/");
bool downloadFile(const std::string& url, const std::string& path);
bool downloadToBuffer(const std::string& url, std::string& buffer);
