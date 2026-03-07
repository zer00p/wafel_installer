#pragma once

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
const wchar_t* getCFWVersionName(CFWVersion version);
bool isStroopwafelAvailable();
bool wasStroopwafelDownloadedInSession();
void setStroopwafelDownloadedInSession(bool downloaded);
bool isIsfshaxInstalled();
bool checkIsfshaxInstalled();
bool isSdEmulated();
std::string getStroopwafelPluginPath();
void setStroopwafelPluginPath(const std::string& path);
bool isShutdownPending();
void setShutdownPending(bool pending, bool forced = false);
bool isShutdownForced();

#define CHECK_SHUTDOWN() if (isShutdownForced()) return;
#define CHECK_SHUTDOWN_VAL(v) if (isShutdownForced()) return (v);

bool isRebootPending();
void setRebootPending(bool pending);

bool isFullRebootPending();
void setFullRebootPending(bool pending);