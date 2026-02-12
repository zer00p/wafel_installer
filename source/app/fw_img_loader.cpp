#include "fw_img_loader.h"
#include "gui.h"
#include "cfw.h"
#include "menu.h"
#include <mocha/mocha.h>
#include <sysapp/title.h>
#include <sysapp/launch.h>
#include <stroopwafel/stroopwafel.h>

#include <cstring>
#include <sstream>
#include <iomanip>

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

static bool applyPatch(uint32_t addr, const void* data, size_t size, const wchar_t* description) {
    WHBLogFreetypePrint(description);
    WHBLogFreetypeDrawScreen();

    for (size_t i = 0; i < size; i += 4) {
        uint32_t word = 0;
        std::memcpy(&word, (const uint8_t*)data + i, size - i < 4 ? size - i : 4);
        MochaUtilsStatus status = Mocha_IOSUKernelWrite32(addr + i, word);
        if (status != MOCHA_RESULT_SUCCESS) {
            std::wstringstream ss;
            ss << L"Failed to write patch at 0x" << std::hex << std::uppercase << std::setw(8) << std::setfill(L'0') << (addr + i) << L"!";
            setErrorPrompt(ss.str());
            showErrorPrompt(L"OK");
            return false;
        }
    }
    return true;
}


void loadFwImg(const char* fwPath) {
    WHBLogFreetypeStartScreen();
    WHBLogFreetypePrint(L"Applying fw.img patches...");
    WHBLogFreetypeDrawScreen();

    if (stroopwafel_available) {
        WHBLogFreetypePrint(L"libstroopwafel is available. Using stroopwafel to change firmware path.");
        Stroopwafel_SetFwPath(fwPath);
        WHBLogFreetypePrint(L"Path changed using libstroopwafel. Skipping patches.");
    } else {
        WHBLogFreetypePrint(L"libstroopwafel not available. Applying fw.img patches...");

        if (!applyPatch(0x050663B4, path, sizeof(path), L"Applying fw_path...")) return;

        uint32_t p2 = 0xF031FB43;
        if (!applyPatch(0x050282AE, &p2, 4, L"Applying patch 2 (launch_os_hook bl)...")) return;

        uint32_t p3 = 0xE3A00000;
        if (!applyPatch(0x05052C44, &p3, 4, L"Applying patch 3 (mov r0, #0)...")) return;

        uint32_t p4 = 0xE12FFF1E;
        if (!applyPatch(0x05052C48, &p4, 4, L"Applying patch 4 (bx lr)...")) return;

        uint32_t p5 = 0x20002000;
        if (!applyPatch(0x0500A818, &p5, 4, L"Applying patch 5 (mov r0, #0; mov r0, #0)...")) return;

        if (!applyPatch(0x05059938, os_launch_hook, sizeof(os_launch_hook), L"Applying os_launch_hook...")) return;

        uint32_t ancast_hook_start = (0x05059938 + sizeof(os_launch_hook) + 3) & ~3;
        if (!applyPatch(ancast_hook_start, ancast_decrypt_hook, sizeof(ancast_decrypt_hook), L"Applying ancast_decrypt_hook...")) return;

        uint32_t p8 = generate_bl_t(0x0500A678, ancast_hook_start);
        if (!applyPatch(0x0500A678, &p8, 4, L"Applying patch 8 (generate_bl_t)...")) return;

        uint32_t p9 = 0xe00fbf00;
        if (!applyPatch(0x0500A7C8, &p9, 4, L"Applying patch 9 (Ancast header nop nop)...")) return;

        uint32_t p10 = 0x2302e003;
        if (!applyPatch(0x0500a7f4, &p10, 4, L"Applying patch 10 (movs r3, #2; b #0x500a800)...")) return;

        // Undo ISFShax fallback reload patch
        uint32_t val = 0; 
        MochaUtilsStatus status = Mocha_IOSUKernelRead32(0x0501f578, &val);
        if (status != MOCHA_RESULT_SUCCESS) {
            WHBLogFreetypePrint(L"Warning: Failed to read ISFShax fallback patch!");
            WHBLogFreetypeDrawScreen();
        } else if (val == 0x32044bcc) {
            uint32_t undo_val = 0xe0914bcc;
            if (!applyPatch(0x0501f578, &undo_val, 4, L"Undoing ISFShax fallback reload patch...")) return;
        }
    }

    WHBLogFreetypeClear();
    if (strcmp(fwPath, "/vol/system/hax/installer/fw.img") == 0) {
        WHBLogFreetypePrint(L"Patches applied. Launching ISFShax Installer...");
    } else {
        WHBLogFreetypePrintf(L"Patches applied. Launching %S...", toWstring(fwPath).c_str());
    }
    WHBLogFreetypeDraw();
    sleep_for(3s);

    shutdownCFW();

    OSForceFullRelaunch();
    SYSLaunchMenu();

    exitApplication(false);
    _Exit(0);
}
