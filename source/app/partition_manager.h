#pragma once

#include <coreinit/filesystem_fsa.h>
#include <string>

void formatAndPartitionMenu();
void setupSDUSBMenu();
bool formatWholeDrive(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo);
bool partitionDevice(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo);
std::wstring getDeviceSummary(FSAClientHandle fsaHandle, const char* device, const FSADeviceInfo& deviceInfo);
void usbAsSd(bool enable);
