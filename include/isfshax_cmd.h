#ifndef ISFSHAX_CMD_H
#define ISFSHAX_CMD_H

#include <stdint.h>

#define ISFSHAX_CMD_ADDR 0xFFFFFF00

#define ISFSHAX_CMD_MAGIC "FSHAXCMD"

#define ISFSHAX_CMD_INSTALL   0x494E5354
#define ISFSHAX_CMD_UNINSTALL 0x554E494E

#define ISFSHAX_CMD_SOURCE_SD   0
#define ISFSHAX_CMD_SOURCE_SLC  1

#define ISFSHAX_CMD_POST_NOTHING   0
#define ISFSHAX_CMD_POST_REBOOT    1
#define ISFSHAX_CMD_POST_POWEROFF  2

typedef struct {
    char magic[8];
    uint32_t command;
    uint32_t parameter;
} isfshax_cmd;

#endif
