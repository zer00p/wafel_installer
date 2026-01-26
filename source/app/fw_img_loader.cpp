#include "fw_img_loader.h"
#include "gui.h"
#include "cfw.h"
#include <mocha/mocha.h>
#include <sysapp/title.h>
#include <sysapp/launch.h>
#include <stroopwafel/stroopwafel.h>




/* from smealum's iosuhax: must be placed at 0x05059938
	0x0000000005059938:  47 78    bx   pc
	0x000000000505993a:  00 00
	0x000000000505993c:  E9 2D 40 0F    push  {r0, r1, r2, r3, lr}
	0x0000000005059940:  E2 4D D0 08    sub   sp, sp, #8
	0x0000000005059944:  EB FF FD FD    bl    #0x5059140
	0x0000000005059948:  E3 A0 00 00    mov   r0, #0
	0x000000000505994c:  EB FF FE 03    bl    #0x5059160
	0x0000000005059950:  E5 9F 10 4C    ldr   r1, [pc, #0x4c] @ -> 050599a4 -> 05059970 -> "/dev/sdcard01"
	0x0000000005059954:  E5 9F 20 4C    ldr   r2, [pc, #0x4c] @ -> 050599a8 -> 0505997E -> "/vol/sdcard"
	0x0000000005059958:  E3 A0 30 00    mov   r3, #0
	0x000000000505995c:  E5 8D 30 00    str   r3, [sp]
	0x0000000005059960:  E5 8D 30 04    str   r3, [sp, #4]
	0x0000000005059964:  EB FF FE F1    bl    #0x5059530
	0x0000000005059968:  E2 8D D0 08    add   sp, sp, #8
	0x000000000505996c:  E8 BD 80 0F    pop   {r0, r1, r2, r3, pc}
	0x0000000005059970:  "/dev/sdcard01",0
	0x000000000505997e:  "/vol/sdcard",0,0,0
	0x000000000505998c:  "/vol/sdcard",0
	0x0000000005059998:  05 11 60 00
	0x000000000505999c:  05 0B E0 00
	0x00000000050599a0:  05 0B CF FC
	0x00000000050599a4:  05 05 99 70
	0x00000000050599a8:  05 05 99 7E

*/
static const char os_launch_hook[] = {
	0x47, 0x78, 0x00, 0x00, 0xe9, 0x2d, 0x40, 0x0f, 0xe2, 0x4d, 0xd0, 0x08, 0xeb,
	0xff, 0xfd, 0xfd, 0xe3, 0xa0, 0x00, 0x00, 0xeb, 0xff, 0xfe, 0x03, 0xe5, 0x9f,
	0x10, 0x4c, 0xe5, 0x9f, 0x20, 0x4c, 0xe3, 0xa0, 0x30, 0x00, 0xe5, 0x8d, 0x30,
	0x00, 0xe5, 0x8d, 0x30, 0x04, 0xeb, 0xff, 0xfe, 0xf1, 0xe2, 0x8d, 0xd0, 0x08,
	0xe8, 0xbd, 0x80, 0x0f, 0x2f, 0x64, 0x65, 0x76, 0x2f, 0x73, 0x64, 0x63, 0x61,
	0x72, 0x64, 0x30, 0x31, 0x00, 0x2f, 0x76, 0x6f, 0x6c, 0x2f, 0x73, 0x64, 0x63,
	0x61, 0x72, 0x64, 0x00, 0x00, 0x00, 0x2f, 0x76, 0x6f, 0x6c, 0x2f, 0x73, 0x64,
	0x63, 0x61, 0x72, 0x64, 0x00, 0x05, 0x11, 0x60, 0x00, 0x05, 0x0b, 0xe0, 0x00,
	0x05, 0x0b, 0xcf, 0xfc, 0x05, 0x05, 0x99, 0x70, 0x05, 0x05, 0x99, 0x7e,
};

/* from stoopwafel, allows unencrypted fw.img
ancast_crypt_check:
    .thumb
    bx pc
    nop
    .arm
    ldr r7, =0x010001A0 @ device type offset
    ldrh r7, [r7]       @ get device type
    tst r7, #1          @ set bit 0 at the u16 at 0x1A0 for no-crypt mode
    bne ancast_no_crypt
    add r7, sp, #0x24
    str r7, [sp, #0x18]
    bx lr
ancast_no_crypt:
    pop {r4-r7, lr}
    add sp, #0x10
    mov r0, #0
    bx lr
*/
static const char ancast_decrypt_hook[] = {
	0x47, 0x78, 0xbf, 0x00,
	0xe5, 0x9f, 0x70, 0x24, 0xe1, 0xd7, 0x70, 0xb0,
    0xe3, 0x17, 0x00, 0x01, 0x1a, 0x00, 0x00, 0x02,
    0xe2, 0x8d, 0x70, 0x24, 0xe5, 0x8d, 0x70, 0x18,
    0xe1, 0x2f, 0xff, 0x1e, 0xe8, 0xbd, 0x40, 0xf0,
    0xe2, 0x8d, 0xd0, 0x10, 0xe3, 0xa0, 0x00, 0x00,
    0xe1, 0x2f, 0xff, 0x1e, 0x01, 0x00, 0x01, 0xa0,
};

static const char path[64] = "/vol/system/hax/installer";

static uint32_t generate_bl_t(uint32_t from, uint32_t to)
{
	int32_t bl_offs = (((int32_t)to - (int32_t)(from)) - 4) / 2;
	uint32_t bl_insn = 0xF000F800 | ((uint32_t)bl_offs & 0x7FF) | ((((uint32_t)bl_offs >> 11) & 0x3FF) << 16);
	return bl_insn;
}


void loadFwImg() {
    WHBLogFreetypeStartScreen();
    WHBLogFreetypePrint(L"Applying fw.img patches...");
    WHBLogFreetypeDrawScreen();

    MochaUtilsStatus status;

    status = Mocha_InitLibrary();
    if (status != MOCHA_RESULT_SUCCESS) {
        WHBLogFreetypeClear();
        WHBLogFreetypePrint(L"Failed to initialize Mocha!");
        WHBLogFreetypePrintf(L"Error code: %d", status);
        WHBLogFreetypeDraw();
        sleep_for(5s);
        return;
    }

    WHBLogFreetypePrint(L"Applying fw_path...");
    WHBLogFreetypeDrawScreen();
    // Patches from unencrypted_cfw_booter - sd_path
    uint32_t target_address = 0x050663B4;

    for (int i = 0; i < sizeof(path); i+=sizeof(uint32_t)) {
        status = Mocha_IOSUKernelWrite32(0x050663B4 + i, *(uint32_t*)(path+i));
        if (status != MOCHA_RESULT_SUCCESS) {
            WHBLogFreetypeClear();
            WHBLogFreetypePrintf(L"Failed to write word for sd_path patch at 0x%08X!", 0x050663B4 + i);
            WHBLogFreetypePrintf(L"Error code: %d", status);
            WHBLogFreetypeDraw();
            sleep_for(5s);
            Mocha_DeInitLibrary();
            return;
        }
    }

    WHBLogFreetypePrint(L"Applying patch 2 (launch_os_hook bl)...");
    WHBLogFreetypeDrawScreen();
    status = Mocha_IOSUKernelWrite32(0x050282AE, 0xF031FB43); // bl launch_os_hook
    if (status != MOCHA_RESULT_SUCCESS) {
        WHBLogFreetypeClear();
        WHBLogFreetypePrintf(L"Failed to apply patch 2 at 0x%08X!", 0x050282AE);
        WHBLogFreetypePrintf(L"Error code: %d", status);
        WHBLogFreetypeDraw();
        sleep_for(5s);
        Mocha_DeInitLibrary();
        return;
    }

    WHBLogFreetypePrint(L"Applying patch 3 (mov r0, #0)...");
    WHBLogFreetypeDrawScreen();
    status = Mocha_IOSUKernelWrite32(0x05052C44, 0xE3A00000); // mov r0, #0
    if (status != MOCHA_RESULT_SUCCESS) {
        WHBLogFreetypeClear();
        WHBLogFreetypePrintf(L"Failed to apply patch 3 at 0x%08X!", 0x05052C44);
        WHBLogFreetypePrintf(L"Error code: %d", status);
        WHBLogFreetypeDraw();
        sleep_for(5s);
        Mocha_DeInitLibrary();
        return;
    }

    WHBLogFreetypePrint(L"Applying patch 4 (bx lr)...");
    WHBLogFreetypeDrawScreen();
    status = Mocha_IOSUKernelWrite32(0x05052C48, 0xE12FFF1E); // bx lr
    if (status != MOCHA_RESULT_SUCCESS) {
        WHBLogFreetypeClear();
        WHBLogFreetypePrintf(L"Failed to apply patch 4 at 0x%08X!", 0x05052C48);
        WHBLogFreetypePrintf(L"Error code: %d", status);
        WHBLogFreetypeDraw();
        sleep_for(5s);
        Mocha_DeInitLibrary();
        return;
    }

    WHBLogFreetypePrint(L"Applying patch 5 (mov r0, #0; mov r0, #0)...");
    WHBLogFreetypeDrawScreen();
    status = Mocha_IOSUKernelWrite32(0x0500A818, 0x20002000); // mov r0, #0; mov r0, #0
    if (status != MOCHA_RESULT_SUCCESS) {
        WHBLogFreetypeClear();
        WHBLogFreetypePrintf(L"Failed to apply patch 5 at 0x%08X!", 0x0500A818);
        WHBLogFreetypePrintf(L"Error code: %d", status);
        WHBLogFreetypeDraw();
        sleep_for(5s);
        Mocha_DeInitLibrary();
        return;
    }

    WHBLogFreetypePrint(L"Applying os_launch_hook...");
    WHBLogFreetypeDrawScreen();
    uint32_t os_launch_hook_target_addr = 0x05059938;
    for (size_t i = 0; i < sizeof(os_launch_hook); i += 4) {
        // Ensure that we are reading 4 bytes at a time
        uint32_t word = *(uint32_t*)(os_launch_hook + i);
        status = Mocha_IOSUKernelWrite32(os_launch_hook_target_addr + i, word);
        if (status != MOCHA_RESULT_SUCCESS) {
            WHBLogFreetypeClear();
            WHBLogFreetypePrintf(L"Failed to write word for os_launch_hook at 0x%08X!", os_launch_hook_target_addr + i);
            WHBLogFreetypePrintf(L"Error code: %d", status);
            WHBLogFreetypeDraw();
            sleep_for(5s);
            Mocha_DeInitLibrary();
            return;
        }
    }

    WHBLogFreetypePrint(L"Applying ancast_decrypt_hook...");
    WHBLogFreetypeDrawScreen();
    uint32_t ancast_hook_start = (0x05059938 + sizeof(os_launch_hook) + 3) & ~3;
    uint32_t ancast_decrypt_hook_target_addr = ancast_hook_start;
    for (size_t i = 0; i < sizeof(ancast_decrypt_hook); i += 4) {
        // Ensure that we are reading 4 bytes at a time
        uint32_t word = *(uint32_t*)(ancast_decrypt_hook + i);
        status = Mocha_IOSUKernelWrite32(ancast_decrypt_hook_target_addr + i, word);
        if (status != MOCHA_RESULT_SUCCESS) {
            WHBLogFreetypeClear();
            WHBLogFreetypePrintf(L"Failed to write word for ancast_decrypt_hook at 0x%08X!", ancast_decrypt_hook_target_addr + i);
            WHBLogFreetypePrintf(L"Error code: %d", status);
            WHBLogFreetypeDraw();
            sleep_for(5s);
            Mocha_DeInitLibrary();
            return;
        }
    }

    WHBLogFreetypePrint(L"Applying patch 8 (generate_bl_t)...");
    WHBLogFreetypeDrawScreen();
    status = Mocha_IOSUKernelWrite32(0x0500A678, generate_bl_t(0x0500A678, ancast_hook_start));
    if (status != MOCHA_RESULT_SUCCESS) {
        WHBLogFreetypeClear();
        WHBLogFreetypePrintf(L"Failed to apply patch 8 at 0x%08X!", 0x0500A678);
        WHBLogFreetypePrintf(L"Error code: %d", status);
        WHBLogFreetypeDraw();
        sleep_for(5s);
        Mocha_DeInitLibrary();
        return;
    }

    WHBLogFreetypePrint(L"Applying patch 9 (Ancast header nop nop)...");
    status = Mocha_IOSUKernelWrite32(0x0500A7C8, 0xe00fbf00); // b #0x500a7ea
    if (status != MOCHA_RESULT_SUCCESS) {
        WHBLogFreetypeClear();
        WHBLogFreetypePrintf(L"Failed to apply patch 9 (second part) at 0x%08X!", 0x0500A7C8);
        WHBLogFreetypePrintf(L"Error code: %d", status);
        WHBLogFreetypeDraw();
        sleep_for(5s);
        Mocha_DeInitLibrary();
        return;
    }

    WHBLogFreetypePrint(L"Applying patch 10 (movs r3, #2; b #0x500a800)...");
    WHBLogFreetypeDrawScreen();
    status = Mocha_IOSUKernelWrite32(0x0500a7f4, 0x2302e003); // movs r3, #2;  b #0x500a800
    if (status != MOCHA_RESULT_SUCCESS) {
        WHBLogFreetypeClear();
        WHBLogFreetypePrintf(L"Failed to apply patch 10 at 0x%08X!", 0x0500a7f4);
        WHBLogFreetypePrintf(L"Error code: %d", status);
        WHBLogFreetypeDraw();
        sleep_for(5s);
        Mocha_DeInitLibrary();
        return;
    }

    Mocha_DeInitLibrary();

    WHBLogFreetypeClear();
    WHBLogFreetypePrint(L"Patches applied. Launching system menu...");
    WHBLogFreetypeDraw();
    sleep_for(3s);

    OSForceFullRelaunch();
    SYSLaunchMenu();

    exitApplication(false);
    _Exit(0);

    return;
}
