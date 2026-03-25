#ifndef RTOUCH_H
#define RTOUCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rtouch_port.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    RTOUCH_EVENT_NONE        = 0u,
    RTOUCH_EVENT_TOUCH_DOWN  = 1u << 0,
    RTOUCH_EVENT_TOUCH_UP    = 1u << 1,
    RTOUCH_EVENT_TAP         = 1u << 2,
    RTOUCH_EVENT_DOUBLE_TAP  = 1u << 3,
    RTOUCH_EVENT_HOLD        = 1u << 4
};

typedef struct {
    uint16_t adc_max;
    uint8_t samples_per_axis;
    uint16_t settle_time_us;
    uint16_t consistency_threshold;
    uint8_t touch_confirm_samples;
    uint8_t release_confirm_samples;
    uint8_t filter_shift;
    uint16_t tap_move_limit;
    uint16_t hold_move_limit;
    uint16_t tap_max_duration_ms;
    uint16_t hold_delay_ms;
    uint16_t double_tap_gap_ms;
    uint16_t accel_start;
    uint16_t accel_slope;
    uint16_t accel_max_gain_q8;
} rtouch_config_t;

typedef struct {
    bool touching;
    uint16_t x;
    uint16_t y;
    int16_t dx;
    int16_t dy;
    uint8_t events;
    uint32_t timestamp_ms;
} rtouch_state_t;

typedef struct {
    rtouch_port_t port;
    rtouch_config_t config;
    rtouch_state_t state;

    uint32_t touch_start_ms;
    uint32_t last_tap_release_ms;
    uint16_t touch_start_x;
    uint16_t touch_start_y;
    uint16_t filtered_x;
    uint16_t filtered_y;
    uint8_t valid_count;
    uint8_t invalid_count;
    bool hold_reported;
    bool tap_armed;
} rtouch_t;

void rtouch_init(rtouch_t *ctx, const rtouch_port_t *port, const rtouch_config_t *config);
void rtouch_get_default_config(rtouch_config_t *config);
void rtouch_reset(rtouch_t *ctx);
const rtouch_state_t *rtouch_get_state(const rtouch_t *ctx);
bool rtouch_update(rtouch_t *ctx);

#ifdef __cplusplus
}
#endif

#endif
