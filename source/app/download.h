#pragma once

#include <string>

bool downloadHaxFiles();
bool downloadHaxFilesToSD();
bool download5sdusb(bool toSLC, bool toSD);
bool download5upartsd(bool toSLC);
bool downloadInstallerOnly();
bool downloadAroma(const std::string& sdPath = "fs:/vol/external01/");
