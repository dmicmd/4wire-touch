#include "rtouch.h"

#include <string.h>

#define RTOUCH_COORD_MAX 65535u
#define RTOUCH_MAX_SAMPLES 7u

static uint16_t rtouch_abs_diff_u16(uint16_t a, uint16_t b)
{
    return (a > b) ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

static uint16_t rtouch_median_u16(uint16_t *values, uint8_t count)
{
    for (uint8_t i = 1; i < count; ++i) {
        uint16_t key = values[i];
        uint8_t j = i;
        while ((j > 0u) && (values[j - 1u] > key)) {
            values[j] = values[j - 1u];
            --j;
        }
        values[j] = key;
    }
    return values[count / 2u];
}

static uint16_t rtouch_scale_adc(const rtouch_t *ctx, uint16_t sample)
{
    if (ctx->config.adc_max == 0u) {
        return 0u;
    }

    uint32_t scaled = ((uint32_t)sample * RTOUCH_COORD_MAX) / ctx->config.adc_max;
    if (scaled > RTOUCH_COORD_MAX) {
        scaled = RTOUCH_COORD_MAX;
    }
    return (uint16_t)scaled;
}

static void rtouch_set_all_analog(rtouch_t *ctx)
{
    for (uint8_t wire = 0; wire < (uint8_t)RTOUCH_WIRE_COUNT; ++wire) {
        ctx->port.set_wire_mode(ctx->port.user_data, (rtouch_wire_t)wire, RTOUCH_PIN_MODE_ANALOG);
    }
}

static uint16_t rtouch_sample_axis(rtouch_t *ctx,
                                   rtouch_wire_t hi_wire,
                                   rtouch_wire_t lo_wire,
                                   rtouch_wire_t sense_wire)
{
    rtouch_set_all_analog(ctx);
    ctx->port.set_wire_mode(ctx->port.user_data, hi_wire, RTOUCH_PIN_MODE_OUTPUT_HIGH);
    ctx->port.set_wire_mode(ctx->port.user_data, lo_wire, RTOUCH_PIN_MODE_OUTPUT_LOW);
    ctx->port.set_wire_mode(ctx->port.user_data, sense_wire, RTOUCH_PIN_MODE_ANALOG);
    ctx->port.delay_us(ctx->port.user_data, ctx->config.settle_time_us);
    return ctx->port.read_wire_adc(ctx->port.user_data, sense_wire);
}

static bool rtouch_read_raw(rtouch_t *ctx,
                            uint16_t *x_out,
                            uint16_t *y_out)
{
    uint8_t count = ctx->config.samples_per_axis;
    if (count == 0u) {
        count = 1u;
    }
    if (count > RTOUCH_MAX_SAMPLES) {
        count = RTOUCH_MAX_SAMPLES;
    }

    uint16_t xf[RTOUCH_MAX_SAMPLES];
    uint16_t xr[RTOUCH_MAX_SAMPLES];
    uint16_t yf[RTOUCH_MAX_SAMPLES];
    uint16_t yr[RTOUCH_MAX_SAMPLES];

    for (uint8_t i = 0; i < count; ++i) {
        xf[i] = rtouch_sample_axis(ctx, RTOUCH_WIRE_XP, RTOUCH_WIRE_XM, RTOUCH_WIRE_YP);
        xr[i] = rtouch_sample_axis(ctx, RTOUCH_WIRE_XM, RTOUCH_WIRE_XP, RTOUCH_WIRE_YP);
        yf[i] = rtouch_sample_axis(ctx, RTOUCH_WIRE_YP, RTOUCH_WIRE_YM, RTOUCH_WIRE_XP);
        yr[i] = rtouch_sample_axis(ctx, RTOUCH_WIRE_YM, RTOUCH_WIRE_YP, RTOUCH_WIRE_XP);
    }

    uint16_t x_forward = rtouch_median_u16(xf, count);
    uint16_t x_reverse = rtouch_median_u16(xr, count);
    uint16_t y_forward = rtouch_median_u16(yf, count);
    uint16_t y_reverse = rtouch_median_u16(yr, count);

    uint16_t x_mirror = (x_reverse >= ctx->config.adc_max)
        ? 0u
        : (uint16_t)(ctx->config.adc_max - x_reverse);
    uint16_t y_mirror = (y_reverse >= ctx->config.adc_max)
        ? 0u
        : (uint16_t)(ctx->config.adc_max - y_reverse);

    if (rtouch_abs_diff_u16(x_forward, x_mirror) > ctx->config.consistency_threshold) {
        return false;
    }
    if (rtouch_abs_diff_u16(y_forward, y_mirror) > ctx->config.consistency_threshold) {
        return false;
    }

    *x_out = (uint16_t)(((uint32_t)x_forward + x_mirror) / 2u);
    *y_out = (uint16_t)(((uint32_t)y_forward + y_mirror) / 2u);
    return true;
}

static int16_t rtouch_apply_acceleration(const rtouch_t *ctx, int32_t delta)
{
    int32_t magnitude = (delta < 0) ? -delta : delta;
    uint32_t gain_q8 = 256u;

    if ((uint32_t)magnitude > ctx->config.accel_start) {
        uint32_t extra = (uint32_t)magnitude - ctx->config.accel_start;
        gain_q8 += extra * ctx->config.accel_slope;
        if (gain_q8 > ctx->config.accel_max_gain_q8) {
            gain_q8 = ctx->config.accel_max_gain_q8;
        }
    }

    int32_t accelerated = (delta * (int32_t)gain_q8) / 256;
    if (accelerated > INT16_MAX) {
        accelerated = INT16_MAX;
    }
    if (accelerated < INT16_MIN) {
        accelerated = INT16_MIN;
    }
    return (int16_t)accelerated;
}

static uint16_t rtouch_manhattan(uint16_t ax, uint16_t ay, uint16_t bx, uint16_t by)
{
    return (uint16_t)(rtouch_abs_diff_u16(ax, bx) + rtouch_abs_diff_u16(ay, by));
}

void rtouch_get_default_config(rtouch_config_t *config)
{
    memset(config, 0, sizeof(*config));
    config->adc_max = 4095u;
    config->samples_per_axis = 5u;
    config->settle_time_us = 30u;
    config->consistency_threshold = 160u;
    config->touch_confirm_samples = 2u;
    config->release_confirm_samples = 2u;
    config->filter_shift = 2u;
    config->tap_move_limit = 2200u;
    config->hold_move_limit = 3200u;
    config->tap_max_duration_ms = 180u;
    config->hold_delay_ms = 450u;
    config->double_tap_gap_ms = 280u;
    config->accel_start = 220u;
    config->accel_slope = 2u;
    config->accel_max_gain_q8 = 1024u;
}

void rtouch_reset(rtouch_t *ctx)
{
    memset(&ctx->state, 0, sizeof(ctx->state));
    ctx->touch_start_ms = 0u;
    ctx->last_tap_release_ms = 0u;
    ctx->touch_start_x = 0u;
    ctx->touch_start_y = 0u;
    ctx->filtered_x = 0u;
    ctx->filtered_y = 0u;
    ctx->valid_count = 0u;
    ctx->invalid_count = 0u;
    ctx->hold_reported = false;
    ctx->tap_armed = false;
    rtouch_set_all_analog(ctx);
}

void rtouch_init(rtouch_t *ctx, const rtouch_port_t *port, const rtouch_config_t *config)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->port = *port;
    if (config != NULL) {
        ctx->config = *config;
    } else {
        rtouch_get_default_config(&ctx->config);
    }
    rtouch_reset(ctx);
}

const rtouch_state_t *rtouch_get_state(const rtouch_t *ctx)
{
    return &ctx->state;
}

bool rtouch_update(rtouch_t *ctx)
{
    uint16_t raw_x = 0u;
    uint16_t raw_y = 0u;
    uint32_t now_ms = ctx->port.get_time_ms(ctx->port.user_data);
    uint8_t events = 0u;

    bool valid = rtouch_read_raw(ctx, &raw_x, &raw_y);
    if (valid) {
        ctx->valid_count = (ctx->valid_count < UINT8_MAX) ? (uint8_t)(ctx->valid_count + 1u) : UINT8_MAX;
        ctx->invalid_count = 0u;
    } else {
        ctx->invalid_count = (ctx->invalid_count < UINT8_MAX) ? (uint8_t)(ctx->invalid_count + 1u) : UINT8_MAX;
        ctx->valid_count = 0u;
    }

    if (!ctx->state.touching) {
        ctx->state.dx = 0;
        ctx->state.dy = 0;

        if (valid && (ctx->valid_count >= ctx->config.touch_confirm_samples)) {
            ctx->state.touching = true;
            ctx->touch_start_ms = now_ms;
            ctx->state.timestamp_ms = now_ms;
            ctx->filtered_x = rtouch_scale_adc(ctx, raw_x);
            ctx->filtered_y = rtouch_scale_adc(ctx, raw_y);
            ctx->state.x = ctx->filtered_x;
            ctx->state.y = ctx->filtered_y;
            ctx->touch_start_x = ctx->state.x;
            ctx->touch_start_y = ctx->state.y;
            ctx->hold_reported = false;
            ctx->tap_armed = true;
            events |= RTOUCH_EVENT_TOUCH_DOWN;
        }

        ctx->state.events = events;
        return ctx->state.touching;
    }

    if (!valid && (ctx->invalid_count >= ctx->config.release_confirm_samples)) {
        uint32_t duration = now_ms - ctx->touch_start_ms;
        uint16_t motion = rtouch_manhattan(ctx->touch_start_x, ctx->touch_start_y, ctx->state.x, ctx->state.y);

        ctx->state.touching = false;
        ctx->state.dx = 0;
        ctx->state.dy = 0;
        ctx->state.timestamp_ms = now_ms;
        events |= RTOUCH_EVENT_TOUCH_UP;

        if (ctx->tap_armed && !ctx->hold_reported &&
            (duration <= ctx->config.tap_max_duration_ms) &&
            (motion <= ctx->config.tap_move_limit)) {
            if ((ctx->last_tap_release_ms != 0u) &&
                ((now_ms - ctx->last_tap_release_ms) <= ctx->config.double_tap_gap_ms)) {
                events |= RTOUCH_EVENT_DOUBLE_TAP;
                ctx->last_tap_release_ms = 0u;
            } else {
                events |= RTOUCH_EVENT_TAP;
                ctx->last_tap_release_ms = now_ms;
            }
        }

        ctx->state.events = events;
        rtouch_set_all_analog(ctx);
        return false;
    }

    if (valid) {
        uint16_t scaled_x = rtouch_scale_adc(ctx, raw_x);
        uint16_t scaled_y = rtouch_scale_adc(ctx, raw_y);

        if (ctx->config.filter_shift != 0u) {
            ctx->filtered_x = (uint16_t)(ctx->filtered_x + ((int32_t)scaled_x - ctx->filtered_x) / (1u << ctx->config.filter_shift));
            ctx->filtered_y = (uint16_t)(ctx->filtered_y + ((int32_t)scaled_y - ctx->filtered_y) / (1u << ctx->config.filter_shift));
        } else {
            ctx->filtered_x = scaled_x;
            ctx->filtered_y = scaled_y;
        }

        int32_t raw_dx = (int32_t)ctx->filtered_x - ctx->state.x;
        int32_t raw_dy = (int32_t)ctx->filtered_y - ctx->state.y;

        ctx->state.dx = rtouch_apply_acceleration(ctx, raw_dx);
        ctx->state.dy = rtouch_apply_acceleration(ctx, raw_dy);
        ctx->state.x = ctx->filtered_x;
        ctx->state.y = ctx->filtered_y;
        ctx->state.timestamp_ms = now_ms;

        if (!ctx->hold_reported &&
            ((now_ms - ctx->touch_start_ms) >= ctx->config.hold_delay_ms) &&
            (rtouch_manhattan(ctx->touch_start_x, ctx->touch_start_y, ctx->state.x, ctx->state.y) <= ctx->config.hold_move_limit)) {
            ctx->hold_reported = true;
            ctx->tap_armed = false;
            events |= RTOUCH_EVENT_HOLD;
        }
    } else {
        ctx->state.dx = 0;
        ctx->state.dy = 0;
        ctx->state.timestamp_ms = now_ms;
    }

    ctx->state.events = events;
    return true;
}
