#include "ff.h"
#include "diskio.h"
#include <coreinit/filesystem.h>
#include <coreinit/debug.h>
#include <coreinit/time.h>
#include <stdlib.h>
#include <mocha/mocha.h>
#include <mocha/fsa.h>
#include <coreinit/filesystem_fsa.h>

const char* fatDevPaths[FF_VOLUMES] = {"/dev/sdcard01", "/dev/usb01", "/dev/usb02", "/dev/usb03"};
bool fatMounted[FF_VOLUMES] = {false, false, false, false};
FSAClientHandle fatClients[FF_VOLUMES] = {0, 0, 0, 0};
IOSHandle fatHandles[FF_VOLUMES] = {-1, -1, -1, -1};
const WORD fatSectorSizes[FF_VOLUMES] = {512, 512, 512, 512};

DSTATUS wiiu_mountDrive(BYTE pdrv) {
    if (pdrv >= FF_VOLUMES) return STA_NOINIT;
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
    if (pdrv >= FF_VOLUMES) return STA_NOINIT;
    if (fatMounted[pdrv]) {
        FSAEx_RawCloseEx(fatClients[pdrv], fatHandles[pdrv]);
        FSADelClient(fatClients[pdrv]);
        fatMounted[pdrv] = false;
        fatClients[pdrv] = 0;
        fatHandles[pdrv] = -1;
    }
    return 0;
}

DSTATUS disk_status (BYTE pdrv) {
    if (pdrv >= FF_VOLUMES) return STA_NOINIT;
    if (!fatMounted[pdrv]) return STA_NOINIT;
    return 0;
}

DSTATUS disk_initialize (BYTE pdrv) {
    if (pdrv >= FF_VOLUMES) return STA_NOINIT;
    if (fatMounted[pdrv]) return 0;
    return wiiu_mountDrive(pdrv);
}

DRESULT disk_read (BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv >= FF_VOLUMES || !fatMounted[pdrv]) return RES_NOTRDY;
    FSError status = FSAEx_RawReadEx(fatClients[pdrv], buff, fatSectorSizes[pdrv], count, sector, fatHandles[pdrv]);
    return (status == FS_ERROR_OK) ? RES_OK : RES_ERROR;
}

DRESULT disk_write (BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv >= FF_VOLUMES || !fatMounted[pdrv]) return RES_NOTRDY;
    FSError status = FSAEx_RawWriteEx(fatClients[pdrv], (void*)buff, fatSectorSizes[pdrv], count, sector, fatHandles[pdrv]);
    return (status == FS_ERROR_OK) ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void *buff) {
    if (pdrv >= FF_VOLUMES || !fatMounted[pdrv]) return RES_NOTRDY;
    switch (cmd) {
        case CTRL_SYNC: return RES_OK;
        case GET_SECTOR_COUNT: {
             FSADeviceInfo deviceInfo = {};
             if (FSAGetDeviceInfo(fatClients[pdrv], fatDevPaths[pdrv], &deviceInfo) != FS_ERROR_OK) return RES_ERROR;
             *(LBA_t*)buff = deviceInfo.deviceSizeInSectors;
             return RES_OK;
        }
        case GET_SECTOR_SIZE: *(WORD*)buff = fatSectorSizes[pdrv]; return RES_OK;
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
