#pragma once
#include "commands.h"
#include <coreinit/filesystem.h>
#include <coreinit/filesystem_fsa.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum StroopwafelStatus {
    STROOPWAFEL_RESULT_SUCCESS                 = 0,
    STROOPWAFEL_RESULT_INVALID_ARGUMENT        = -0x01,
    STROOPWAFEL_RESULT_MAX_CLIENT              = -0x02,
    STROOPWAFEL_RESULT_OUT_OF_MEMORY           = -0x03,
    STROOPWAFEL_RESULT_ALREADY_EXISTS          = -0x04,
    STROOPWAFEL_RESULT_ADD_DEVOPTAB_FAILED     = -0x05,
    STROOPWAFEL_RESULT_NOT_FOUND               = -0x06,
    STROOPWAFEL_RESULT_UNSUPPORTED_API_VERSION = -0x10,
    STROOPWAFEL_RESULT_UNSUPPORTED_COMMAND     = -0x11,
    STROOPWAFEL_RESULT_UNSUPPORTED_CFW         = -0x12,
    STROOPWAFEL_RESULT_LIB_UNINITIALIZED       = -0x20,
    STROOPWAFEL_RESULT_UNKNOWN_ERROR           = -0x100,
} StroopwafelStatus;

const char *Stroopwafel_GetStatusStr(StroopwafelStatus status);

/**
 * Initializes the stroopwafel lib. Needs to be called before any other functions can be used
 * @return STROOPWAFEL_RESULT_SUCCESS:                Library has been successfully initialized <br>
 *         STROOPWAFEL_RESULT_UNSUPPORTED_COMMAND:    Failed to initialize the library caused by an outdated stroopwafel version.
 */
StroopwafelStatus Stroopwafel_InitLibrary();

/**
 * Deinitializes the stroopwafel lib
 * @return
 */
StroopwafelStatus Stroopwafel_DeInitLibrary();

/**
 * Retrieves the API Version of the running stroopwafel.
 *
 * @param outVersion pointer to the variable where the version will be stored.
 *
 * @return STROOPWAFEL_RESULT_SUCCESS:                    The API version has been store in the version ptr<br>
 *         STROOPWAFEL_RESULT_INVALID_ARGUMENT:           Invalid version pointer<br>
 *         STROOPWAFEL_RESULT_UNSUPPORTED_API_VERSION:    Failed to get the API version caused by an outdated stroopwafel version.<br>
 *         STROOPWAFEL_RESULT_UNSUPPORTED_CFW:            Failed to get the API version caused by not using a (compatible) CFW.
 */
StroopwafelStatus Stroopwafel_GetAPIVersion(uint32_t *outVersion);

/**
 * Sets the fw.img path.
 * @param path The full path for the fw.img (e.g., "/vol/sdcard/minute/fw.img").
 * @return STROOPWAFEL_RESULT_SUCCESS: The path has been set successfully.
 *         STROOPWAFEL_RESULT_INVALID_ARGUMENT: Invalid path pointer.
 *         STROOPWAFEL_RESULT_LIB_UNINITIALIZED: Library was not initialized. Call Stroopwafel_InitLibrary() before using this function.
 *         STROOPWAFEL_RESULT_UNKNOWN_ERROR: Unknown error.
 */
StroopwafelStatus Stroopwafel_SetFwPath(const char *path);

typedef struct StroopwafelWrite {
    uint32_t dest_addr;
    const void *src;
    uint32_t length;
} StroopwafelWrite;

/**
 * Writes data to the IOS memory.
 * @param num_writes The number of writes to perform.
 * @param writes Pointer to an array of StroopwafelWrite structures.
 * @return STROOPWAFEL_RESULT_SUCCESS: The memory has been written successfully.
 *         STROOPWAFEL_RESULT_INVALID_ARGUMENT: Invalid arguments or too many writes.
 *         STROOPWAFEL_RESULT_LIB_UNINITIALIZED: Library was not initialized.
 *         STROOPWAFEL_RESULT_UNKNOWN_ERROR: Unknown error.
 */
StroopwafelStatus Stroopwafel_WriteMemory(uint32_t num_writes, const StroopwafelWrite *writes);

/**
 * Executes code at a target address in IOS.
 * @param target_addr The address to execute.
 * @param config Optional configuration buffer to pass as the first argument.
 * @param config_len Length of the configuration buffer.
 * @param output Optional output buffer to pass as the second argument.
 * @param output_len Length of the output buffer.
 * @return STROOPWAFEL_RESULT_SUCCESS: Execution completed.
 *         STROOPWAFEL_RESULT_INVALID_ARGUMENT: Invalid target address.
 *         STROOPWAFEL_RESULT_LIB_UNINITIALIZED: Library was not initialized.
 *         STROOPWAFEL_RESULT_UNKNOWN_ERROR: Unknown error.
 */
StroopwafelStatus Stroopwafel_Execute(uint32_t target_addr, const void *config, uint32_t config_len, void *output, uint32_t output_len);

typedef struct StroopwafelMapMemory {
    uint32_t paddr;
    uint32_t vaddr;
    uint32_t size;
    uint32_t domain;
    uint32_t type;
    uint32_t cached;
} StroopwafelMapMemory;

/**
 * Maps memory pages in IOS.
 * @param info Pointer to a StroopwafelMapMemory structure.
 * @return STROOPWAFEL_RESULT_SUCCESS: Memory mapped successfully.
 *         STROOPWAFEL_RESULT_INVALID_ARGUMENT: Invalid info pointer.
 *         STROOPWAFEL_RESULT_LIB_UNINITIALIZED: Library was not initialized.
 *         STROOPWAFEL_RESULT_UNKNOWN_ERROR: Unknown error.
 */
StroopwafelStatus Stroopwafel_MapMemory(const StroopwafelMapMemory *info);

typedef enum StroopwafelMinuteDevice {
    STROOPWAFEL_MIN_DEV_UNKNOWN = 0,
    STROOPWAFEL_MIN_DEV_SLC     = 1,
    STROOPWAFEL_MIN_DEV_SD      = 2,
} StroopwafelMinuteDevice;

typedef struct StroopwafelMinutePath {
    uint32_t device;
    char path[256];
} StroopwafelMinutePath;

/**
 * Retrieves the path to the minute binary.
 * @param out Pointer to a StroopwafelMinutePath structure where the path will be stored.
 * @return STROOPWAFEL_RESULT_SUCCESS: Path retrieved successfully.
 *         STROOPWAFEL_RESULT_INVALID_ARGUMENT: Invalid out pointer.
 *         STROOPWAFEL_RESULT_LIB_UNINITIALIZED: Library was not initialized.
 *         STROOPWAFEL_RESULT_UNKNOWN_ERROR: Unknown error.
 */
StroopwafelStatus Stroopwafel_GetMinutePath(StroopwafelMinutePath *out);

/**
 * Retrieves the path to the plugins directory.
 * @param out Pointer to a StroopwafelMinutePath structure where the path will be stored.
 * @return STROOPWAFEL_RESULT_SUCCESS: Path retrieved successfully.
 *         STROOPWAFEL_RESULT_INVALID_ARGUMENT: Invalid out pointer.
 *         STROOPWAFEL_RESULT_LIB_UNINITIALIZED: Library was not initialized.
 *         STROOPWAFEL_RESULT_UNKNOWN_ERROR: Unknown error.
 */
StroopwafelStatus Stroopwafel_GetPluginPath(StroopwafelMinutePath *out);


#ifdef __cplusplus
} // extern "C"
#endif
