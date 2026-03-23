#include "rtouch.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    uint32_t now_ms;
    bool touched;
    uint16_t x_adc;
    uint16_t y_adc;
    rtouch_pin_mode_t mode[RTOUCH_WIRE_COUNT];
} fake_hw_t;

static void fake_set_wire_mode(void *user_data, rtouch_wire_t wire, rtouch_pin_mode_t mode)
{
    fake_hw_t *hw = (fake_hw_t *)user_data;
    hw->mode[wire] = mode;
}

static bool fake_is_high(const fake_hw_t *hw, rtouch_wire_t wire)
{
    return hw->mode[wire] == RTOUCH_PIN_MODE_OUTPUT_HIGH;
}

static bool fake_is_low(const fake_hw_t *hw, rtouch_wire_t wire)
{
    return hw->mode[wire] == RTOUCH_PIN_MODE_OUTPUT_LOW;
}

static uint16_t fake_read_wire_adc(void *user_data, rtouch_wire_t wire)
{
    fake_hw_t *hw = (fake_hw_t *)user_data;
    if (!hw->touched) {
        return 0u;
    }

    if (wire == RTOUCH_WIRE_YP) {
        if (fake_is_high(hw, RTOUCH_WIRE_XP) && fake_is_low(hw, RTOUCH_WIRE_XM)) {
            return hw->x_adc;
        }
        if (fake_is_high(hw, RTOUCH_WIRE_XM) && fake_is_low(hw, RTOUCH_WIRE_XP)) {
            return (uint16_t)(4095u - hw->x_adc);
        }
    }

    if (wire == RTOUCH_WIRE_XP) {
        if (fake_is_high(hw, RTOUCH_WIRE_YP) && fake_is_low(hw, RTOUCH_WIRE_YM)) {
            return hw->y_adc;
        }
        if (fake_is_high(hw, RTOUCH_WIRE_YM) && fake_is_low(hw, RTOUCH_WIRE_YP)) {
            return (uint16_t)(4095u - hw->y_adc);
        }
    }

    return 0u;
}

static void fake_delay_us(void *user_data, uint32_t usec)
{
    (void)user_data;
    (void)usec;
}

static uint32_t fake_get_time_ms(void *user_data)
{
    return ((fake_hw_t *)user_data)->now_ms;
}

static void test_single_tap(void)
{
    fake_hw_t hw = {0};
    rtouch_port_t port = {
        .user_data = &hw,
        .set_wire_mode = fake_set_wire_mode,
        .read_wire_adc = fake_read_wire_adc,
        .delay_us = fake_delay_us,
        .get_time_ms = fake_get_time_ms,
    };
    rtouch_config_t cfg;
    rtouch_get_default_config(&cfg);
    cfg.touch_confirm_samples = 1u;
    cfg.release_confirm_samples = 1u;
    cfg.filter_shift = 0u;
    rtouch_t touch;
    rtouch_init(&touch, &port, &cfg);

    hw.touched = true;
    hw.x_adc = 1024u;
    hw.y_adc = 2048u;
    rtouch_update(&touch);
    assert(touch.state.touching);
    assert((touch.state.events & RTOUCH_EVENT_TOUCH_DOWN) != 0u);

    hw.now_ms = 60u;
    rtouch_update(&touch);

    hw.now_ms = 100u;
    hw.touched = false;
    rtouch_update(&touch);
    assert(!touch.state.touching);
    assert((touch.state.events & RTOUCH_EVENT_TAP) != 0u);
}

static void test_double_tap(void)
{
    fake_hw_t hw = {0};
    rtouch_port_t port = {
        .user_data = &hw,
        .set_wire_mode = fake_set_wire_mode,
        .read_wire_adc = fake_read_wire_adc,
        .delay_us = fake_delay_us,
        .get_time_ms = fake_get_time_ms,
    };
    rtouch_config_t cfg;
    rtouch_get_default_config(&cfg);
    cfg.touch_confirm_samples = 1u;
    cfg.release_confirm_samples = 1u;
    cfg.filter_shift = 0u;
    rtouch_t touch;
    rtouch_init(&touch, &port, &cfg);

    for (int i = 0; i < 2; ++i) {
        hw.touched = true;
        hw.x_adc = 1500u;
        hw.y_adc = 1500u;
        rtouch_update(&touch);
        hw.now_ms += 50u;
        rtouch_update(&touch);
        hw.touched = false;
        hw.now_ms += 20u;
        rtouch_update(&touch);
        hw.now_ms += 120u;
    }

    assert((touch.state.events & RTOUCH_EVENT_DOUBLE_TAP) != 0u);
}

static void test_hold_and_acceleration(void)
{
    fake_hw_t hw = {0};
    rtouch_port_t port = {
        .user_data = &hw,
        .set_wire_mode = fake_set_wire_mode,
        .read_wire_adc = fake_read_wire_adc,
        .delay_us = fake_delay_us,
        .get_time_ms = fake_get_time_ms,
    };
    rtouch_config_t cfg;
    rtouch_get_default_config(&cfg);
    cfg.touch_confirm_samples = 1u;
    cfg.release_confirm_samples = 1u;
    cfg.filter_shift = 0u;
    rtouch_t touch;
    rtouch_init(&touch, &port, &cfg);

    hw.touched = true;
    hw.x_adc = 600u;
    hw.y_adc = 700u;
    rtouch_update(&touch);

    hw.now_ms = cfg.hold_delay_ms + 5u;
    rtouch_update(&touch);
    assert((touch.state.events & RTOUCH_EVENT_HOLD) != 0u);

    uint16_t old_x = touch.state.x;
    hw.now_ms += 5u;
    hw.x_adc = 3200u;
    hw.y_adc = 700u;
    rtouch_update(&touch);
    assert(touch.state.x > old_x);
    assert(touch.state.dx > 0);
}

int main(void)
{
    test_single_tap();
    test_double_tap();
    test_hold_and_acceleration();
    puts("rtouch tests passed");
    return 0;
}
