#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STROOPWAFEL_IOCTL_GET_API_VERSION 0x1
#define STROOPWAFEL_IOCTL_SET_FW_PATH     0x2
#define STROOPWAFEL_IOCTLV_WRITE_MEMORY   0x3
#define STROOPWAFEL_IOCTLV_EXECUTE        0x4
#define STROOPWAFEL_IOCTL_MAP_MEMORY      0x5
#define STROOPWAFEL_IOCTL_GET_MINUTE_PATH 0x6
#define STROOPWAFEL_IOCTL_GET_PLUGIN_PATH 0x7

#define STROOPWAFEL_API_VERSION           0x010000 // v1.0.0

#ifdef __cplusplus
} // extern "C"
#endif
