/*
 * NSO GameCube USB Probe for Wii/vWii (IOS58, devkitPro/libogc)
 * Experimental hardware probe only; NOT a Nintendont patch.
 *
 * It enumerates the same device's USB HID interface (class 0x03)
 * and vendor interface (class 0xFF), sends SDL3's volatile USB
 * initialization commands to vendor endpoints 02/82, and then
 * observes HID reports on endpoint 81.
 *
 * This program never writes firmware/SPI flash.
 */
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/usb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nsogc_protocol.h"

static u8 vendor_tx[64] ATTRIBUTE_ALIGN(32);
static u8 vendor_rx[64] ATTRIBUTE_ALIGN(32);
static u8 hid_rx[64] ATTRIBUTE_ALIGN(32);
static usb_device_entry vendor_list[32] ATTRIBUTE_ALIGN(32);
static usb_device_entry hid_list[32] ATTRIBUTE_ALIGN(32);
static volatile int async_done = 0;
static volatile s32 async_result = -999;

static s32 on_usb_reply(s32 result, void *userdata) {
    (void)userdata;
    async_result = result;
    async_done = 1;
    return 0;
}

static int await_reply(int timeout_ms) {
    int waited;
    for (waited = 0; waited < timeout_ms; waited += 10) {
        if (async_done) return async_result;
        usleep(10000);
    }
    return -999;
}

static s32 find_device(u8 class_code, usb_device_entry *entries) {
    u8 count = 0;
    s32 result = -1;
    unsigned tries, i;
    for(tries = 0; tries < 25; tries++) {
        count = 0;
        memset(entries, 0, 32 * sizeof(usb_device_entry));
        result = USB_GetDeviceList(entries, 32, class_code, &count);
        if(result >= 0) {
            for(i = 0; i < count && i < 32; ++i) {
                if(entries[i].vid == NSOGC_VID && entries[i].pid == NSOGC_PID)
                    return (s32)entries[i].device_id;
            }
        }
        usleep(100000);
    }
    return -1;
}

static void dump_report(const u8 *buf, int size) {
    struct nsogc_state s;
    int i;
    printf("\x1b[2J\x1b[H");
    printf("NSO GameCube USB Probe - vWii / IOS58\n\n");
    printf("HID report, length=%d, type=%02X\n", size, buf[0]);
    printf("Bytes: ");
    for(i=0; i<16 && i<size; ++i) printf("%02X ", buf[i]);
    printf("\n\n");
    if(nsogc_decode(buf, (size_t)size, &s) == 0) {
        printf("A:%d B:%d X:%d Y:%d START:%d Z:%d Lclick:%d Rclick:%d\n",
            s.a,s.b,s.x,s.y,s.start,s.z,s.l_click,s.r_click);
        printf("D-pad U:%d D:%d L:%d R:%d  C:%d SHARE:%d\n",
            s.up,s.down,s.left,s.right,s.c,s.share);
        printf("Left stick : %4u %4u  (neutral ~2048)\n",s.lx,s.ly);
        printf("Right stick: %4u %4u  (neutral ~2048)\n",s.rx,s.ry);
        printf("Triggers: L:%3u R:%3u\n",s.lt,s.rt);
    } else {
        printf("No report 0A yet (not initialized or different format).\n");
    }
    printf("\nHOME on Wii Remote to exit, or power off.\n");
}

int main(int argc, char **argv) {
    s32 vendor_id, hid_id, vendor_fd = -1, hid_fd = -1;
    int r, i, nreports=0;
    u8 first_byte = 0;
    GXRModeObj *rmode;
    void *xfb;
    (void)argc; (void)argv;

    VIDEO_Init();
    rmode = VIDEO_GetPreferredMode(NULL);
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    console_init(xfb,20,20,rmode->fbWidth-40,rmode->xfbHeight-40,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    WPAD_Init();

    printf("NSO GameCube USB Probe - experimental\n");
    printf("Controller: Nintendo 057E:2073\n");
    printf("Searching interface 1 (vendor) and interface 0 (HID)...\n\n");
    r = USB_Initialize();
    printf("USB_Initialize: %ld\n", (long)r);
    if(r < 0) goto finish;
    vendor_id = find_device(0xff, vendor_list);
    hid_id = find_device(0x03, hid_list);
    printf("Vendor class 0xFF device ID: %ld\n", (long)vendor_id);
    printf("HID class 0x03 device ID: %ld\n", (long)hid_id);
    if(vendor_id < 0) {
        printf("\nSTOP: Vendor interface not visible to IOS58.\n");
        printf("This is the key missing capability to investigate.\n");
        goto finish;
    }
    r = USB_OpenDevice(vendor_id, NSOGC_VID, NSOGC_PID, &vendor_fd);
    printf("Open vendor interface: %ld, fd %ld\n", (long)r, (long)vendor_fd);
    if(r < 0 || vendor_fd < 0) goto finish;

    printf("\nRunning 10 volatile initialization commands...\n");
    for (i = 0; i < (int)nsogc_init_command_count; ++i) {
        const struct nsogc_command *cmd = &nsogc_init_commands[i];
        memset(vendor_tx, 0, sizeof(vendor_tx));
        memcpy(vendor_tx, cmd->bytes, cmd->length);
        DCFlushRange(vendor_tx, sizeof(vendor_tx));
        r = USB_WriteBlkMsg(vendor_fd, NSOGC_VENDOR_OUT, cmd->length, vendor_tx);
        printf("%2d. %-18s OUT:%ld", i+1, cmd->label, (long)r);
        if(r < 0) { printf(" FAILED\n"); goto finish; }
        memset(vendor_rx, 0, sizeof(vendor_rx));
        DCFlushRange(vendor_rx, sizeof(vendor_rx));
        async_done = 0;
        async_result = -999;
        r = USB_ReadBlkMsgAsync(vendor_fd, NSOGC_VENDOR_IN, sizeof(vendor_rx), vendor_rx, on_usb_reply, NULL);
        if(r < 0) { printf(" IN-queue:%ld FAILED\n", (long)r); goto finish; }
        r = await_reply(1200);
        if(r == -999) {
            printf(" REPLY TIMEOUT\n");
            printf("USB operation may remain pending. Restart vWii.\n");
            goto finish;
        }
        DCInvalidateRange(vendor_rx, sizeof(vendor_rx));
        printf(" IN:%ld [ %02X %02X %02X %02X ]\n", (long)r,
               vendor_rx[0],vendor_rx[1],vendor_rx[2],vendor_rx[3]);
        if(r < 0) goto finish;
        VIDEO_WaitVSync();
    }

    printf("\nVendor setup done! Now opening HID interface 0.\n");
    if(hid_id < 0) { printf("HID interface missing; stop.\n"); goto finish; }
    r = USB_OpenDevice(hid_id, NSOGC_VID, NSOGC_PID, &hid_fd);
    printf("Open HID interface: %ld, fd %ld\n", (long)r, (long)hid_fd);
    if(r < 0 || hid_fd < 0) goto finish;
    printf("Press buttons and move sticks on GameCube NSO.\n");
    usleep(350000);
    for (i = 0; i < 1800; ++i) { /* maximum ~3 min */
        WPAD_ScanPads();
        if(WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) break;
        memset(hid_rx, 0, sizeof(hid_rx));
        DCFlushRange(hid_rx, sizeof(hid_rx));
        async_done = 0;
        async_result = -999;
        r = USB_ReadIntrMsgAsync(hid_fd, NSOGC_HID_IN, sizeof(hid_rx), hid_rx, on_usb_reply, NULL);
        if(r < 0) { printf("HID queue error %ld\n",(long)r); break; }
        r = await_reply(2000);
        if(r == -999) {
            printf("\nNo HID report after 2 s; cancel/restart if needed.\n");
            break;
        }
        if(r < 0) { printf("\nHID read error %ld\n", (long)r); break; }
        DCInvalidateRange(hid_rx, sizeof(hid_rx));
        if(r > 0) {
            first_byte = hid_rx[0];
            dump_report(hid_rx, r);
            ++nreports;
        }
    }
    printf("\nHID reports received: %d; last type %02X\n", nreports, first_byte);

finish:
    printf("\nTake a photo of this screen (especially error codes).\n");
    printf("HOME on Wii Remote to exit, or power off.\n");
    while(1) {
        WPAD_ScanPads();
        if(WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) break;
        usleep(40000);
    }
    if (hid_fd >= 0) USB_CloseDevice(&hid_fd);
    if (vendor_fd >= 0) USB_CloseDevice(&vendor_fd);
    USB_Deinitialize();
    return 0;
}
