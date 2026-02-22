#pragma once

#include <stdint.h>
#include <string_view>
#include "common_paths.h"

extern bool stroopwafel_available;

void loadFwImg(const std::string& fwPath = Paths::SystemHaxInstallerFwImg, uint32_t command = 0, uint32_t parameter = 0);
