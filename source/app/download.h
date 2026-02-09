#pragma once

#include <string>

bool downloadHaxFiles();
bool downloadHaxFilesToSD();
bool download5sdusb(bool toSLC, bool toSD);
bool download5upartsd(bool toSLC);
bool downloadInstallerOnly();
bool downloadAroma(const std::string& sdPath = "fs:/vol/external01/");
bool downloadFile(const std::string& url, const std::string& path);
bool downloadToBuffer(const std::string& url, std::string& buffer);
