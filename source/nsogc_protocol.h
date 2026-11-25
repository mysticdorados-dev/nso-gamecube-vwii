#ifndef NSOGC_PROTOCOL_H
#define NSOGC_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>

/* Nintendo Switch 2 NSO GameCube controller USB VID:PID 057E:2073.
 * Input report 0x0A after initialization; offsets cross-checked against SDL3.
 */
#define NSOGC_VID 0x057e
#define NSOGC_PID 0x2073
#define NSOGC_REPORT_GC 0x0a
#define NSOGC_HID_IN 0x81
#define NSOGC_VENDOR_OUT 0x02
#define NSOGC_VENDOR_IN 0x82

struct nsogc_state {
    uint8_t a, b, x, y, start, z, l_click, r_click;
    uint8_t left_shoulder, right_shoulder, guide, share, c;
    uint8_t up, down, left, right;
    uint16_t lx, ly, rx, ry; /* 0-4095; center approximately 2048 */
    uint8_t lt, rt; /* raw analog trigger travel */
};

struct nsogc_command { const uint8_t *bytes; unsigned length; const char *label; };
extern const struct nsogc_command nsogc_init_commands[];
extern const unsigned nsogc_init_command_count;
int nsogc_decode(const uint8_t *packet, size_t size, struct nsogc_state *out);
#endif
