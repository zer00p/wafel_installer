#include "common.h"

// Functions related to devices
bool mountSystemDrives();
bool mountDisc();
bool mountUsbFat();
bool unmountSystemDrives();
bool unmountDisc();
void unmountUsbFat();

bool formatUsbFat();

bool isDiscMounted();
bool isSlcMounted();
bool testStorage(TITLE_LOCATION location);

// Filesystem helper functions
std::string convertToPosixPath(const char* volPath);
bool fileExist(const char* path);
bool dirExist(const char* path);

TITLE_LOCATION deviceToLocation(const char* device);
TITLE_LOCATION pathToLocation(const char* device);