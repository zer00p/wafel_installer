#pragma once

#include <stdint.h>

extern bool stroopwafel_available;

void loadFwImg(const char* fwPath = "/vol/system/hax/installer/fw.img", uint32_t command = 0, uint32_t parameter = 0);
