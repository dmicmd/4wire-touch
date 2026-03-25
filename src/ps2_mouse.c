#include "ps2_mouse.h"

#include <string.h>

#define PS2_ACK              0xFAu
#define PS2_SELF_TEST_PASS   0xAAu
#define PS2_DEVICE_ID        0x00u
#define PS2_RESEND           0xFEu

#define PS2_CMD_SET_SCALE11  0xE6u
#define PS2_CMD_SET_SCALE21  0xE7u
#define PS2_CMD_SET_RES      0xE8u
#define PS2_CMD_STATUS       0xE9u
#define PS2_CMD_SET_STREAM   0xEAu
#define PS2_CMD_READ_DATA    0xEBu
#define PS2_CMD_RESET_WRAP   0xECu
#define PS2_CMD_SET_WRAP     0xEEu
#define PS2_CMD_SET_REMOTE   0xF0u
#define PS2_CMD_GET_ID       0xF2u
#define PS2_CMD_SET_RATE     0xF3u
#define PS2_CMD_ENABLE       0xF4u
#define PS2_CMD_DISABLE      0xF5u
#define PS2_CMD_DEFAULTS     0xF6u
#define PS2_CMD_RESET        0xFFu

static bool ps2_mouse_queue_byte(ps2_mouse_t *ctx, uint8_t byte)
{
    uint8_t next = (uint8_t)(ctx->tx_head + 1u);
    if (next == ctx->tx_tail) {
        return false;
    }
    ctx->tx_queue[ctx->tx_head] = byte;
    ctx->tx_head = next;
    return true;
}

static void ps2_mouse_queue_packet(ps2_mouse_t *ctx, int16_t dx, int16_t dy)
{
    int16_t send_x = dx;
    int16_t send_y = dy;
    uint8_t status = 0x08u | (ctx->buttons & 0x07u);

    if (send_x > 255) {
        send_x = 255;
        status |= 1u << 6;
    } else if (send_x < -255) {
        send_x = -255;
        status |= 1u << 6;
    }

    if (send_y > 255) {
        send_y = 255;
        status |= 1u << 7;
    } else if (send_y < -255) {
        send_y = -255;
        status |= 1u << 7;
    }

    if (send_x < 0) {
        status |= 1u << 4;
    }
    if (send_y < 0) {
        status |= 1u << 5;
    }

    (void)ps2_mouse_queue_byte(ctx, status);
    (void)ps2_mouse_queue_byte(ctx, (uint8_t)send_x);
    (void)ps2_mouse_queue_byte(ctx, (uint8_t)send_y);
}

static void ps2_mouse_flush_pending(ps2_mouse_t *ctx)
{
    while ((ctx->pending_dx != 0) || (ctx->pending_dy != 0)) {
        int16_t chunk_x = ctx->pending_dx;
        int16_t chunk_y = ctx->pending_dy;

        if (chunk_x > 255) {
            chunk_x = 255;
        } else if (chunk_x < -255) {
            chunk_x = -255;
        }

        if (chunk_y > 255) {
            chunk_y = 255;
        } else if (chunk_y < -255) {
            chunk_y = -255;
        }

        ps2_mouse_queue_packet(ctx, chunk_x, chunk_y);
        ctx->pending_dx = (int16_t)(ctx->pending_dx - chunk_x);
        ctx->pending_dy = (int16_t)(ctx->pending_dy - chunk_y);
    }
}

static void ps2_mouse_queue_status(ps2_mouse_t *ctx)
{
    uint8_t status = ctx->buttons & 0x07u;
    if (ctx->reporting_enabled) {
        status |= 1u << 5;
    }
    if (ctx->scaling_21) {
        status |= 1u << 4;
    }
    if (ctx->remote_mode) {
        status |= 1u << 6;
    }

    (void)ps2_mouse_queue_byte(ctx, status);
    (void)ps2_mouse_queue_byte(ctx, ctx->resolution);
    (void)ps2_mouse_queue_byte(ctx, ctx->sample_rate);
}

void ps2_mouse_reset(ps2_mouse_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->resolution = 2u;
    ctx->sample_rate = 100u;
}

void ps2_mouse_init(ps2_mouse_t *ctx)
{
    ps2_mouse_reset(ctx);
}

void ps2_mouse_move(ps2_mouse_t *ctx, int16_t dx, int16_t dy)
{
    int32_t sum_x = (int32_t)ctx->pending_dx + dx;
    int32_t sum_y = (int32_t)ctx->pending_dy + dy;

    if (sum_x > INT16_MAX) {
        sum_x = INT16_MAX;
    } else if (sum_x < INT16_MIN) {
        sum_x = INT16_MIN;
    }
    if (sum_y > INT16_MAX) {
        sum_y = INT16_MAX;
    } else if (sum_y < INT16_MIN) {
        sum_y = INT16_MIN;
    }

    ctx->pending_dx = (int16_t)sum_x;
    ctx->pending_dy = (int16_t)sum_y;

    if (ctx->reporting_enabled && !ctx->remote_mode) {
        ps2_mouse_flush_pending(ctx);
    }
}

void ps2_mouse_set_buttons(ps2_mouse_t *ctx, uint8_t buttons)
{
    buttons &= 0x07u;
    if (buttons == ctx->buttons) {
        return;
    }
    ctx->buttons = buttons;
    if (ctx->reporting_enabled && !ctx->remote_mode) {
        ps2_mouse_queue_packet(ctx, 0, 0);
    }
}

void ps2_mouse_click(ps2_mouse_t *ctx, uint8_t button_mask, uint8_t count)
{
    for (uint8_t i = 0; i < count; ++i) {
        ps2_mouse_set_buttons(ctx, (uint8_t)(ctx->buttons | button_mask));
        ps2_mouse_set_buttons(ctx, (uint8_t)(ctx->buttons & (uint8_t)(~button_mask)));
    }
}

void ps2_mouse_handle_host_byte(ps2_mouse_t *ctx, uint8_t byte)
{
    if (ctx->pending_argument_cmd != 0u) {
        switch (ctx->pending_argument_cmd) {
        case PS2_CMD_SET_RES:
            ctx->resolution = (uint8_t)(byte & 0x03u);
            (void)ps2_mouse_queue_byte(ctx, PS2_ACK);
            break;
        case PS2_CMD_SET_RATE:
            ctx->sample_rate = byte;
            (void)ps2_mouse_queue_byte(ctx, PS2_ACK);
            break;
        default:
            (void)ps2_mouse_queue_byte(ctx, PS2_RESEND);
            break;
        }
        ctx->pending_argument_cmd = 0u;
        return;
    }

    switch (byte) {
    case PS2_CMD_SET_SCALE11:
        ctx->scaling_21 = false;
        (void)ps2_mouse_queue_byte(ctx, PS2_ACK);
        break;
    case PS2_CMD_SET_SCALE21:
        ctx->scaling_21 = true;
        (void)ps2_mouse_queue_byte(ctx, PS2_ACK);
        break;
    case PS2_CMD_SET_RES:
    case PS2_CMD_SET_RATE:
        ctx->pending_argument_cmd = byte;
        (void)ps2_mouse_queue_byte(ctx, PS2_ACK);
        break;
    case PS2_CMD_STATUS:
        (void)ps2_mouse_queue_byte(ctx, PS2_ACK);
        ps2_mouse_queue_status(ctx);
        break;
    case PS2_CMD_SET_STREAM:
        ctx->remote_mode = false;
        (void)ps2_mouse_queue_byte(ctx, PS2_ACK);
        break;
    case PS2_CMD_READ_DATA:
        (void)ps2_mouse_queue_byte(ctx, PS2_ACK);
        ps2_mouse_queue_packet(ctx, ctx->pending_dx, ctx->pending_dy);
        ctx->pending_dx = 0;
        ctx->pending_dy = 0;
        break;
    case PS2_CMD_SET_REMOTE:
        ctx->remote_mode = true;
        (void)ps2_mouse_queue_byte(ctx, PS2_ACK);
        break;
    case PS2_CMD_GET_ID:
        (void)ps2_mouse_queue_byte(ctx, PS2_ACK);
        (void)ps2_mouse_queue_byte(ctx, PS2_DEVICE_ID);
        break;
    case PS2_CMD_ENABLE:
        ctx->reporting_enabled = true;
        (void)ps2_mouse_queue_byte(ctx, PS2_ACK);
        if (!ctx->remote_mode) {
            ps2_mouse_flush_pending(ctx);
        }
        break;
    case PS2_CMD_DISABLE:
        ctx->reporting_enabled = false;
        (void)ps2_mouse_queue_byte(ctx, PS2_ACK);
        break;
    case PS2_CMD_DEFAULTS:
        ctx->reporting_enabled = false;
        ctx->remote_mode = false;
        ctx->scaling_21 = false;
        ctx->resolution = 2u;
        ctx->sample_rate = 100u;
        ctx->pending_argument_cmd = 0u;
        (void)ps2_mouse_queue_byte(ctx, PS2_ACK);
        break;
    case PS2_CMD_RESET:
        ps2_mouse_reset(ctx);
        (void)ps2_mouse_queue_byte(ctx, PS2_ACK);
        (void)ps2_mouse_queue_byte(ctx, PS2_SELF_TEST_PASS);
        (void)ps2_mouse_queue_byte(ctx, PS2_DEVICE_ID);
        break;
    case PS2_RESEND:
        (void)ps2_mouse_queue_byte(ctx, ctx->last_tx);
        break;
    case PS2_CMD_SET_WRAP:
    case PS2_CMD_RESET_WRAP:
        (void)ps2_mouse_queue_byte(ctx, PS2_ACK);
        break;
    default:
        (void)ps2_mouse_queue_byte(ctx, PS2_RESEND);
        break;
    }
}

bool ps2_mouse_pop_tx_byte(ps2_mouse_t *ctx, uint8_t *byte)
{
    if (ctx->tx_head == ctx->tx_tail) {
        return false;
    }

    *byte = ctx->tx_queue[ctx->tx_tail];
    ctx->tx_tail = (uint8_t)(ctx->tx_tail + 1u);
    ctx->last_tx = *byte;
    return true;
}
