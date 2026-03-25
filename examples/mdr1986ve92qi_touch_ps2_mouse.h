#ifndef MDR1986VE92QI_TOUCH_PS2_MOUSE_H
#define MDR1986VE92QI_TOUCH_PS2_MOUSE_H

#include <stdbool.h>
#include <stdint.h>

#include "ps2_mouse.h"
#include "rtouch.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    rtouch_t touch;
    ps2_mouse_t mouse;
    uint32_t tick_hi_us;
    uint16_t last_timer_us;
    uint32_t last_ps2_edge_us;
    uint8_t ps2_rx_bit;
    uint8_t ps2_rx_shift;
    uint8_t ps2_rx_parity;
    uint8_t ps2_tx_byte;
    uint8_t ps2_tx_bit;
    bool ps2_tx_active;
    bool hold_drag;
} mdr_touch_ps2_mouse_t;

void mdr_touch_ps2_mouse_init(mdr_touch_ps2_mouse_t *ctx);
void mdr_touch_ps2_mouse_poll(mdr_touch_ps2_mouse_t *ctx);

#ifdef __cplusplus
}
#endif

#endif
