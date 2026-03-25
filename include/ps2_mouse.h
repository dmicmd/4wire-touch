#ifndef PS2_MOUSE_H
#define PS2_MOUSE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    PS2_MOUSE_BTN_LEFT   = 1u << 0,
    PS2_MOUSE_BTN_RIGHT  = 1u << 1,
    PS2_MOUSE_BTN_MIDDLE = 1u << 2
};

typedef struct {
    uint8_t tx_queue[64];
    uint8_t tx_head;
    uint8_t tx_tail;
    uint8_t last_tx;
    uint8_t buttons;
    uint8_t resolution;
    uint8_t sample_rate;
    bool reporting_enabled;
    bool remote_mode;
    bool scaling_21;
    uint8_t pending_argument_cmd;
    int16_t pending_dx;
    int16_t pending_dy;
} ps2_mouse_t;

void ps2_mouse_init(ps2_mouse_t *ctx);
void ps2_mouse_reset(ps2_mouse_t *ctx);
void ps2_mouse_handle_host_byte(ps2_mouse_t *ctx, uint8_t byte);
void ps2_mouse_move(ps2_mouse_t *ctx, int16_t dx, int16_t dy);
void ps2_mouse_set_buttons(ps2_mouse_t *ctx, uint8_t buttons);
void ps2_mouse_click(ps2_mouse_t *ctx, uint8_t button_mask, uint8_t count);
bool ps2_mouse_pop_tx_byte(ps2_mouse_t *ctx, uint8_t *byte);

#ifdef __cplusplus
}
#endif

#endif
