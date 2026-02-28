#pragma once

#include "common.h"
#include <string_view>
#include <dirent.h> // Required for DIR type

// Functions related to devices
bool mountSystemDrives();
bool mountDisc();
bool unmountSystemDrives();
bool unmountDisc();

bool isDiscMounted();
bool isSlcMounted();
bool testStorage(TITLE_LOCATION location);

// Filesystem helper functions
std::string convertToWiiUFsPath(std::string_view volPath);
bool fileExist(const std::string& path);
bool dirExist(const std::string& path);
bool isDirEmpty(const std::string& path);
bool copyFile(const std::string& src, const std::string& dest);
bool moveFile(const std::string& src, const std::string& dest);
bool removeFile(const std::string& path);
bool removeDir(const std::string& path);
bool createDirectories(const std::string& path);

int fileOpen(const std::string& path, int flags, mode_t mode);
FILE* fileFopen(const std::string& path, const char* mode);
DIR* dirOpen(const std::string& path);

bool isSlcPath(const std::string& path);

bool deleteDirContent(const std::string& path);

TITLE_LOCATION deviceToLocation(std::string_view device);
TITLE_LOCATION pathToLocation(std::string_view device);