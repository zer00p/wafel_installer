#include "common.h"

// Functions related to devices
bool mountSystemDrives();
bool mountDisc();
bool unmountSystemDrives();
bool unmountDisc();

bool isDiscMounted();
bool isSlcMounted();
bool testStorage(TITLE_LOCATION location);

// Filesystem helper functions
std::string convertToPosixPath(const char* volPath);
bool fileExist(const char* path);
bool dirExist(const char* path);
bool copyFile(const std::string& src, const std::string& dest);

TITLE_LOCATION deviceToLocation(const char* device);
TITLE_LOCATION pathToLocation(const char* device);