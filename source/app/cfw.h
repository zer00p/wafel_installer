#include "common.h"
#include <mocha/mocha.h>
#include <string>


enum CFWVersion {
    FAILED,
    NONE,
    MOCHA_FSCLIENT,
    CUSTOM_MOCHA,
    DUMPLING,
    CEMU,
};

CFWVersion testCFW();
bool initCFW();
void shutdownCFW();
CFWVersion getCFWVersion();
bool isStroopwafelAvailable();
bool isIsfshaxInstalled();
bool isSdEmulated();
std::string getStroopwafelPluginPosixPath();
void setStroopwafelPluginPosixPath(const std::string& path);
bool isShutdownPending();
void setShutdownPending(bool pending, bool forced = false);
bool isShutdownForced();