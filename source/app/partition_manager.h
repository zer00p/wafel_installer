#pragma once

#include <coreinit/filesystem_fsa.h>
#include <string>
#include "cfw.h"

extern const wchar_t* WFS_FORMAT_REMINDER;

void setupMountGuard(CFWVersion version);
void formatAndPartitionMenu();
void setupSDUSBMenu();
void setupPartitionedUSBMenu();
void showSDUSBMenu();
void showUSBPartitionMenu();
bool uninstallSDUSB();
bool uninstallUSBPartition();
bool deleteMbr(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo);
bool formatWholeDrive(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo);
bool partitionDevice(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo, bool* outCreatedWiiUPartition = nullptr);
bool fixPartitionOrder(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo);
bool checkAndFixPartitionOrder(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo, bool& repartitioned, bool* outHasWiiUPartition = nullptr);
std::wstring getDeviceSummary(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo);
class FatMountGuard {
public:
    FatMountGuard();
    ~FatMountGuard();
    void block();
    void unblock();
    void silent_unblock();
    void setDeviceName(const std::wstring& name);
private:
    bool active;
    std::wstring deviceName;
};

void showDeviceInfoScreen(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo);
bool waitForDevice(FSAClientHandle fsaHandle, const wchar_t* deviceName, FatMountGuard& guard);
void usbAsSd(bool enable);
bool handleSDUSBAction(FSAClientHandle fsaHandle, const FSADeviceInfo& deviceInfo, FatMountGuard& guard, bool* outHasWiiUPartition = nullptr);
bool handlePartitionActionMenu(FSAClientHandle fsaHandle, const FSADeviceInfo& deviceInfo, const wchar_t* deviceTypeName, bool needWFS, bool* outHasWiiUPartition = nullptr);
struct MbrPartitionInfo {
    bool hasFat = false;
    bool hasWfs = false;
    int partitionCount = 0;
    uint32_t lastOccupiedSector = 1;
    bool hasSpace = false;
    uint32_t fatSizeSectors = 0;
    uint32_t wfsSizeSectors = 0;
};

bool getMbrPartitionInfo(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo, uint8_t* mbr, MbrPartitionInfo& info);
bool checkSdCardPartitioning(FSAClientHandle fsaHandle, const FSADeviceInfo& deviceInfo);
