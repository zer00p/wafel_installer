#include "fatfs_devoptab.h"
#include <sys/iosupport.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <mutex>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include "ff.h"
#include "diskio.h"

struct FatfsMount {
    std::string name;
    std::string drive_prefix; // e.g. "1:"
    FATFS *fs;
    devoptab_t *devoptab;
};

// Structure for a file
typedef struct {
    FFFIL fil;
    FatfsMount *mount;
} fatfs_file_t;

// Structure for a directory
typedef struct {
    FFDIR dir;
    FILINFO info;
    FatfsMount *mount;
} fatfs_dir_t;

static std::vector<FatfsMount*> mounted_fs;
static std::mutex mount_mutex;

static int fatfs_to_errno(FRESULT res) {
    switch (res) {
        case FR_OK: return 0;
        case FR_DISK_ERR: return EIO;
        case FR_INT_ERR: return EIO;
        case FR_NOT_READY: return ENODEV;
        case FR_NO_FILE: return ENOENT;
        case FR_NO_PATH: return ENOENT;
        case FR_INVALID_NAME: return EINVAL;
        case FR_DENIED: return EACCES;
        case FR_EXIST: return EEXIST;
        case FR_INVALID_OBJECT: return EINVAL;
        case FR_WRITE_PROTECTED: return EROFS;
        case FR_INVALID_DRIVE: return ENODEV;
        case FR_NOT_ENABLED: return ENODEV;
        case FR_NO_FILESYSTEM: return ENODEV;
        case FR_MKFS_ABORTED: return EIO;
        case FR_TIMEOUT: return ETIMEDOUT;
        case FR_LOCKED: return EBUSY;
        case FR_NOT_ENOUGH_CORE: return ENOMEM;
        case FR_TOO_MANY_OPEN_FILES: return ENFILE;
        case FR_INVALID_PARAMETER: return EINVAL;
        default: return EIO;
    }
}

static FatfsMount* get_mount_from_path(const char *path) {
    std::string p(path);
    size_t colon = p.find(':');
    if (colon == std::string::npos) return nullptr;
    std::string name = p.substr(0, colon);

    std::lock_guard<std::mutex> lock(mount_mutex);
    for (const auto& m : mounted_fs) {
        if (m->name == name) return m;
    }
    return nullptr;
}

static const char* strip_prefix(const char *path) {
    const char *p = strchr(path, ':');
    if (p) return p + 1;
    return path;
}

static int _fatfs_open_r(struct _reent *r, void *fileStruct, const char *path, int flags, int mode) {
    fatfs_file_t *file = (fatfs_file_t *)fileStruct;
    FatfsMount *m = get_mount_from_path(path);
    if (!m) {
        r->_errno = ENODEV;
        return -1;
    }
    file->mount = m;

    BYTE fat_flags = 0;
    int accmode = (flags & O_ACCMODE);
    if (accmode == O_RDONLY) fat_flags |= FA_READ;
    else if (accmode == O_WRONLY) fat_flags |= FA_WRITE;
    else if (accmode == O_RDWR) fat_flags |= (FA_READ | FA_WRITE);

    if (flags & O_CREAT) {
        if (flags & O_EXCL) fat_flags |= FA_CREATE_NEW;
        else if (flags & O_TRUNC) fat_flags |= FA_CREATE_ALWAYS;
        else fat_flags |= FA_OPEN_ALWAYS;
    } else {
        if (flags & O_TRUNC) fat_flags |= FA_CREATE_ALWAYS;
        else fat_flags |= FA_OPEN_EXISTING;
    }

    if (flags & O_APPEND) fat_flags |= FA_OPEN_APPEND;

    FRESULT res = f_open(&file->fil, m->fs, strip_prefix(path), fat_flags);
    if (res != FR_OK) {
        r->_errno = fatfs_to_errno(res);
        return -1;
    }

    return 0;
}

static int _fatfs_close_r(struct _reent *r, void *fd) {
    fatfs_file_t *file = (fatfs_file_t *)fd;
    FRESULT res = f_close(&file->fil);
    if (res != FR_OK) {
        r->_errno = fatfs_to_errno(res);
        return -1;
    }
    return 0;
}

static ssize_t _fatfs_read_r(struct _reent *r, void *fd, char *ptr, size_t len) {
    fatfs_file_t *file = (fatfs_file_t *)fd;
    UINT read = 0;
    FRESULT res = f_read(&file->fil, ptr, len, &read);
    if (res != FR_OK) {
        r->_errno = fatfs_to_errno(res);
        return -1;
    }
    return (ssize_t)read;
}

static ssize_t _fatfs_write_r(struct _reent *r, void *fd, const char *ptr, size_t len) {
    fatfs_file_t *file = (fatfs_file_t *)fd;
    UINT written = 0;
    FRESULT res = f_write(&file->fil, (void*)ptr, len, &written);
    if (res != FR_OK) {
        r->_errno = fatfs_to_errno(res);
        return -1;
    }
    return (ssize_t)written;
}

static off_t _fatfs_seek_r(struct _reent *r, void *fd, off_t pos, int dir) {
    fatfs_file_t *file = (fatfs_file_t *)fd;
    FSIZE_t target_pos = 0;

    switch (dir) {
        case SEEK_SET: target_pos = pos; break;
        case SEEK_CUR: target_pos = f_tell(&file->fil) + pos; break;
        case SEEK_END: target_pos = f_size(&file->fil) + pos; break;
        default: r->_errno = EINVAL; return -1;
    }

    FRESULT res = f_lseek(&file->fil, target_pos);
    if (res != FR_OK) {
        r->_errno = fatfs_to_errno(res);
        return -1;
    }
    return (off_t)f_tell(&file->fil);
}

static int _fatfs_fstat_r(struct _reent *r, void *fd, struct stat *st) {
    fatfs_file_t *file = (fatfs_file_t *)fd;
    memset(st, 0, sizeof(struct stat));
    st->st_size = f_size(&file->fil);
    st->st_mode = S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    st->st_nlink = 1;
    st->st_blksize = 512;
    st->st_blocks = (st->st_size + 511) / 512;
    return 0;
}

static int _fatfs_stat_r(struct _reent *r, const char *path, struct stat *st) {
    FatfsMount *m = get_mount_from_path(path);
    if (!m) { r->_errno = ENODEV; return -1; }
    FILINFO info;
    FRESULT res = f_stat(m->fs, strip_prefix(path), &info);
    if (res != FR_OK) {
        r->_errno = fatfs_to_errno(res);
        return -1;
    }

    memset(st, 0, sizeof(struct stat));
    st->st_size = info.fsize;
    st->st_mode = (info.fattrib & AM_DIR) ? S_IFDIR : S_IFREG;
    st->st_mode |= S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    st->st_nlink = 1;
    st->st_blksize = 512;
    st->st_blocks = (st->st_size + 511) / 512;

    return 0;
}

static int _fatfs_unlink_r(struct _reent *r, const char *path) {
    FatfsMount *m = get_mount_from_path(path);
    if (!m) { r->_errno = ENODEV; return -1; }
    FRESULT res = f_unlink(m->fs, strip_prefix(path), 0); // 0 = files and directories
    if (res != FR_OK) {
        r->_errno = fatfs_to_errno(res);
        return -1;
    }
    return 0;
}

static int _fatfs_chdir_r(struct _reent *r, const char *path) {
    FatfsMount *m = get_mount_from_path(path);
    if (!m) { r->_errno = ENODEV; return -1; }
    FRESULT res = f_chdir(m->fs, strip_prefix(path));
    if (res != FR_OK) {
        r->_errno = fatfs_to_errno(res);
        return -1;
    }
    return 0;
}

static int _fatfs_rename_r(struct _reent *r, const char *oldName, const char *newName) {
    FatfsMount *m = get_mount_from_path(oldName);
    if (!m) { r->_errno = ENODEV; return -1; }
    // newName should also be on the same mount.
    FRESULT res = f_rename(m->fs, strip_prefix(oldName), strip_prefix(newName));
    if (res != FR_OK) {
        r->_errno = fatfs_to_errno(res);
        return -1;
    }
    return 0;
}

static int _fatfs_mkdir_r(struct _reent *r, const char *path, int mode) {
    FatfsMount *m = get_mount_from_path(path);
    if (!m) { r->_errno = ENODEV; return -1; }
    FRESULT res = f_mkdir(m->fs, strip_prefix(path));
    if (res != FR_OK) {
        r->_errno = fatfs_to_errno(res);
        return -1;
    }
    return 0;
}

static DIR_ITER* _fatfs_diropen_r(struct _reent *r, DIR_ITER *dirState, const char *path) {
    fatfs_dir_t *dir = (fatfs_dir_t *)(dirState->dirStruct);
    FatfsMount *m = get_mount_from_path(path);
    if (!m) { r->_errno = ENODEV; return NULL; }
    dir->mount = m;

    FRESULT res = f_opendir(&dir->dir, m->fs, strip_prefix(path));
    if (res != FR_OK) {
        r->_errno = fatfs_to_errno(res);
        return NULL;
    }
    return dirState;
}

static int _fatfs_dirclose_r(struct _reent *r, DIR_ITER *dirState) {
    fatfs_dir_t *dir = (fatfs_dir_t *)(dirState->dirStruct);
    FRESULT res = f_closedir(&dir->dir);
    if (res != FR_OK) {
        r->_errno = fatfs_to_errno(res);
        return -1;
    }
    return 0;
}

static int _fatfs_dirnext_r(struct _reent *r, DIR_ITER *dirState, char *filename, struct stat *st) {
    fatfs_dir_t *dir = (fatfs_dir_t *)(dirState->dirStruct);
    FRESULT res = f_readdir(&dir->dir, &dir->info);
    if (res != FR_OK) {
        r->_errno = fatfs_to_errno(res);
        return -1;
    }

    if (dir->info.fname[0] == 0) return -1; // End of directory

    strncpy(filename, dir->info.fname, NAME_MAX);
    if (st) {
        memset(st, 0, sizeof(struct stat));
        st->st_size = dir->info.fsize;
        st->st_mode = (dir->info.fattrib & AM_DIR) ? S_IFDIR : S_IFREG;
        st->st_mode |= S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
        st->st_nlink = 1;
        st->st_blksize = 512;
        st->st_blocks = (st->st_size + 511) / 512;
    }

    return 0;
}

static const devoptab_t fatfs_devoptab = {
    NULL, // name
    sizeof(fatfs_file_t),
    _fatfs_open_r,
    _fatfs_close_r,
    _fatfs_write_r,
    _fatfs_read_r,
    _fatfs_seek_r,
    _fatfs_fstat_r,
    _fatfs_stat_r,
    NULL, // link_r
    _fatfs_unlink_r,
    _fatfs_chdir_r,
    _fatfs_rename_r,
    _fatfs_mkdir_r,
    sizeof(fatfs_dir_t),
    _fatfs_diropen_r,
    _fatfs_dirclose_r,
    _fatfs_dirnext_r,
    NULL, // mount_r
    NULL, // umount_r
    NULL, // lstat_r
    NULL  // utimes_r
};

bool fatfs_mount(const std::string& name, int pdrv) {
    std::lock_guard<std::mutex> lock(mount_mutex);

    for (const auto& m : mounted_fs) {
        if (m->name == name) return true;
    }

    FatfsMount *m = new FatfsMount();
    m->name = name;
    m->drive_prefix = std::to_string(pdrv) + ":";
    m->fs = (FATFS *)malloc(sizeof(FATFS));

    FRESULT res = f_mount(m->fs, (void*)m->drive_prefix.c_str(), 1);
    if (res != FR_OK) {
        free(m->fs);
        delete m;
        return false;
    }

    m->devoptab = (devoptab_t *)malloc(sizeof(devoptab_t));
    memcpy(m->devoptab, &fatfs_devoptab, sizeof(devoptab_t));
    m->devoptab->name = strdup(name.c_str());
    m->devoptab->deviceData = m;

    if (AddDevice(m->devoptab) < 0) {
        f_mount(NULL, (void*)m->drive_prefix.c_str(), 0);
        free((void*)m->devoptab->name);
        free(m->devoptab);
        free(m->fs);
        delete m;
        return false;
    }

    mounted_fs.push_back(m);
    return true;
}

bool fatfs_unmount(const std::string& name) {
    std::lock_guard<std::mutex> lock(mount_mutex);
    for (auto it = mounted_fs.begin(); it != mounted_fs.end(); ++it) {
        if ((*it)->name == name) {
            f_mount(NULL, (void*)(*it)->drive_prefix.c_str(), 0);
            RemoveDevice(name.c_str());
            free((void*)(*it)->devoptab->name);
            free((*it)->devoptab);
            free((*it)->fs);
            delete *it;
            mounted_fs.erase(it);
            return true;
        }
    }
    return false;
}
