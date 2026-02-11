#pragma once

#include <coreinit/filesystem_fsa.h>
#include <string>

void formatAndPartitionMenu();
void setupSDUSBMenu();
void setupPartitionedUSBMenu();
bool formatWholeDrive(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo);
bool partitionDevice(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo);
bool fixPartitionOrder(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo);
bool checkAndFixPartitionOrder(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo, bool& repartitioned);
std::wstring getDeviceSummary(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo);
void showDeviceInfoScreen(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo);
bool waitForDevice(FSAClientHandle fsaHandle, const wchar_t* deviceName);
void usbAsSd(bool enable);
