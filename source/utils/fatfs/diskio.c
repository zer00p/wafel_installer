#include "ff.h"
#include "diskio.h"
#include <coreinit/filesystem.h>
#include <coreinit/debug.h>
#include <coreinit/time.h>
#include <stdlib.h>
#include <string.h>
#include <mocha/mocha.h>
#include <mocha/fsa.h>
#include <coreinit/filesystem_fsa.h>

#define INTERNAL_VOLUMES 4
const char* fatDevPaths[INTERNAL_VOLUMES] = {"/dev/sdcard01", "/dev/usb01", "/dev/usb02", "/dev/usb03"};
bool fatMounted[INTERNAL_VOLUMES] = {false, false, false, false};
FSAClientHandle fatClients[INTERNAL_VOLUMES] = {0, 0, 0, 0};
IOSHandle fatHandles[INTERNAL_VOLUMES] = {-1, -1, -1, -1};
const WORD fatSectorSizes[INTERNAL_VOLUMES] = {512, 512, 512, 512};

static int get_pdrv_index(void* pdrv) {
    if (!pdrv) return -1;
    // Handle direct indices (e.g. from f_fdisk)
    if ((uintptr_t)pdrv < INTERNAL_VOLUMES) {
        return (int)(uintptr_t)pdrv;
    }
    // pdrv is usually a string like "1:"
    const char* s = (const char*)pdrv;
    if (s[0] >= '0' && s[0] <= '3' && (s[1] == ':' || s[1] == '\0')) {
        return s[0] - '0';
    }
    return -1;
}

DSTATUS wiiu_mountDrive(BYTE pdrv) {
    if (pdrv >= INTERNAL_VOLUMES) return STA_NOINIT;
    fatClients[pdrv] = FSAAddClient(NULL);
    Mocha_UnlockFSClientEx(fatClients[pdrv]);

    FSError res = FSAEx_RawOpenEx(fatClients[pdrv], fatDevPaths[pdrv], &fatHandles[pdrv]);
    if (res < 0) {
        FSADelClient(fatClients[pdrv]);
        fatClients[pdrv] = 0;
        return STA_NODISK;
    }
    fatMounted[pdrv] = true;
    return 0;
}

DSTATUS wiiu_unmountDrive(BYTE pdrv) {
    if (pdrv >= INTERNAL_VOLUMES) return STA_NOINIT;
    if (fatMounted[pdrv]) {
        FSAEx_RawCloseEx(fatClients[pdrv], fatHandles[pdrv]);
        FSADelClient(fatClients[pdrv]);
        fatMounted[pdrv] = false;
        fatClients[pdrv] = 0;
        fatHandles[pdrv] = -1;
    }
    return 0;
}

DSTATUS disk_status (void* pdrv) {
    int idx = get_pdrv_index(pdrv);
    if (idx < 0 || idx >= INTERNAL_VOLUMES) return STA_NOINIT;
    if (!fatMounted[idx]) return STA_NOINIT;
    return 0;
}

DSTATUS disk_initialize (void* pdrv) {
    int idx = get_pdrv_index(pdrv);
    if (idx < 0 || idx >= INTERNAL_VOLUMES) return STA_NOINIT;
    if (fatMounted[idx]) return 0;
    return wiiu_mountDrive((BYTE)idx);
}

DRESULT disk_read (void* pdrv, BYTE *buff, LBA_t sector, UINT count) {
    int idx = get_pdrv_index(pdrv);
    if (idx < 0 || idx >= INTERNAL_VOLUMES || !fatMounted[idx]) return RES_NOTRDY;
    FSError status = FSAEx_RawReadEx(fatClients[idx], buff, fatSectorSizes[idx], count, sector, fatHandles[idx]);
    return (status == FS_ERROR_OK) ? RES_OK : RES_ERROR;
}

DRESULT disk_write (void* pdrv, const BYTE *buff, LBA_t sector, UINT count) {
    int idx = get_pdrv_index(pdrv);
    if (idx < 0 || idx >= INTERNAL_VOLUMES || !fatMounted[idx]) return RES_NOTRDY;
    FSError status = FSAEx_RawWriteEx(fatClients[idx], (void*)buff, fatSectorSizes[idx], count, sector, fatHandles[idx]);
    return (status == FS_ERROR_OK) ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl (void* pdrv, BYTE cmd, void *buff) {
    int idx = get_pdrv_index(pdrv);
    if (idx < 0 || idx >= INTERNAL_VOLUMES || !fatMounted[idx]) return RES_NOTRDY;
    switch (cmd) {
        case CTRL_SYNC: return RES_OK;
        case GET_SECTOR_COUNT: {
             FSADeviceInfo deviceInfo = {};
             if (FSAGetDeviceInfo(fatClients[idx], fatDevPaths[idx], &deviceInfo) != FS_ERROR_OK) return RES_ERROR;
             *(LBA_t*)buff = deviceInfo.deviceSizeInSectors;
             return RES_OK;
        }
        case GET_SECTOR_SIZE: *(WORD*)buff = fatSectorSizes[idx]; return RES_OK;
        case GET_BLOCK_SIZE: *(DWORD*)buff = 1; return RES_OK;
    }
    return RES_PARERR;
}

DWORD get_fattime(void) {
    OSCalendarTime output;
    OSTicksToCalendarTime(OSGetTime(), &output);
    return (DWORD) (output.tm_year - 1980) << 25 |
           (DWORD) (output.tm_mon + 1) << 21 |
           (DWORD) output.tm_mday << 16 |
           (DWORD) output.tm_hour << 11 |
           (DWORD) output.tm_min << 5 |
           (DWORD) output.tm_sec >> 1;
}
