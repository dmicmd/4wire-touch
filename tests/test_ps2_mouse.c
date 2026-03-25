#include "ps2_mouse.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void expect_byte(ps2_mouse_t *mouse, uint8_t expected)
{
    uint8_t actual = 0u;
    assert(ps2_mouse_pop_tx_byte(mouse, &actual));
    assert(actual == expected);
}

static void test_reset_sequence(void)
{
    ps2_mouse_t mouse;
    ps2_mouse_init(&mouse);

    ps2_mouse_handle_host_byte(&mouse, 0xFFu);
    expect_byte(&mouse, 0xFAu);
    expect_byte(&mouse, 0xAAu);
    expect_byte(&mouse, 0x00u);
}

static void test_enable_and_move(void)
{
    ps2_mouse_t mouse;
    ps2_mouse_init(&mouse);

    ps2_mouse_handle_host_byte(&mouse, 0xF4u);
    expect_byte(&mouse, 0xFAu);

    ps2_mouse_move(&mouse, 20, -5);
    expect_byte(&mouse, 0x28u);
    expect_byte(&mouse, 20u);
    expect_byte(&mouse, (uint8_t)-5);
}

static void test_remote_read_and_click(void)
{
    ps2_mouse_t mouse;
    ps2_mouse_init(&mouse);

    ps2_mouse_handle_host_byte(&mouse, 0xF0u);
    expect_byte(&mouse, 0xFAu);

    ps2_mouse_move(&mouse, 10, 3);
    ps2_mouse_set_buttons(&mouse, PS2_MOUSE_BTN_LEFT);
    ps2_mouse_handle_host_byte(&mouse, 0xEBu);

    expect_byte(&mouse, 0xFAu);
    expect_byte(&mouse, 0x09u);
    expect_byte(&mouse, 10u);
    expect_byte(&mouse, 3u);
}

int main(void)
{
    test_reset_sequence();
    test_enable_and_move();
    test_remote_read_and_click();
    puts("ps2 mouse tests passed");
    return 0;
}
