#pragma once
#include <string>

bool fatfs_mount(const std::string& name, int pdrv);
bool fatfs_unmount(const std::string& name);
