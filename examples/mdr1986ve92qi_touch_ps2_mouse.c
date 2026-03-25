#include "mdr1986ve92qi_touch_ps2_mouse.h"

#include "MDR32F9Qx_adc.h"
#include "MDR32F9Qx_port.h"
#include "MDR32F9Qx_rst_clk.h"
#include "MDR32F9Qx_timer.h"

#define TOUCH_WIRE_YM_PIN PORT_Pin_2
#define TOUCH_WIRE_YP_PIN PORT_Pin_3
#define TOUCH_WIRE_XP_PIN PORT_Pin_5
#define TOUCH_WIRE_XM_PIN PORT_Pin_6

#define PS2_DATA_PIN PORT_Pin_1
#define PS2_CLK_PIN  PORT_Pin_3

#define TOUCH_TIMER  MDR_TIMER1
#define TOUCH_ADC_FLAG ADC1_FLAG_END_OF_CONVERSION

static uint16_t mdr_touch_wire_pin(rtouch_wire_t wire)
{
    switch (wire) {
    case RTOUCH_WIRE_XP:
        return TOUCH_WIRE_XP_PIN;
    case RTOUCH_WIRE_XM:
        return TOUCH_WIRE_XM_PIN;
    case RTOUCH_WIRE_YP:
        return TOUCH_WIRE_YP_PIN;
    case RTOUCH_WIRE_YM:
    default:
        return TOUCH_WIRE_YM_PIN;
    }
}

static uint32_t mdr_touch_wire_channel(rtouch_wire_t wire)
{
    switch (wire) {
    case RTOUCH_WIRE_XP:
        return ADC_CH_ADC5;
    case RTOUCH_WIRE_XM:
        return ADC_CH_ADC6;
    case RTOUCH_WIRE_YP:
        return ADC_CH_ADC3;
    case RTOUCH_WIRE_YM:
    default:
        return ADC_CH_ADC2;
    }
}

static void mdr_port_apply(MDR_PORT_TypeDef *port, uint16_t pin, PORT_MODE_TypeDef mode, PORT_OE_TypeDef oe)
{
    PORT_InitTypeDef init;
    PORT_StructInit(&init);
    init.PORT_Pin = pin;
    init.PORT_FUNC = PORT_FUNC_PORT;
    init.PORT_MODE = mode;
    init.PORT_OE = oe;
    init.PORT_PD = PORT_PD_DRIVER;
    init.PORT_PD_SHM = PORT_PD_SHM_OFF;
    init.PORT_PULL_DOWN = PORT_PULL_DOWN_OFF;
    init.PORT_PULL_UP = PORT_PULL_UP_OFF;
    init.PORT_SPEED = PORT_SPEED_MAXFAST;
    init.PORT_GFEN = PORT_GFEN_OFF;
    PORT_Init(port, &init);
}

static void mdr_touch_set_wire_mode(void *user_data, rtouch_wire_t wire, rtouch_pin_mode_t mode)
{
    (void)user_data;
    uint16_t pin = mdr_touch_wire_pin(wire);

    switch (mode) {
    case RTOUCH_PIN_MODE_OUTPUT_HIGH:
        mdr_port_apply(MDR_PORTD, pin, PORT_MODE_DIGITAL, PORT_OE_OUT);
        PORT_SetBits(MDR_PORTD, pin);
        break;
    case RTOUCH_PIN_MODE_OUTPUT_LOW:
        mdr_port_apply(MDR_PORTD, pin, PORT_MODE_DIGITAL, PORT_OE_OUT);
        PORT_ResetBits(MDR_PORTD, pin);
        break;
    case RTOUCH_PIN_MODE_INPUT:
        mdr_port_apply(MDR_PORTD, pin, PORT_MODE_DIGITAL, PORT_OE_IN);
        break;
    case RTOUCH_PIN_MODE_ANALOG:
    default:
        mdr_port_apply(MDR_PORTD, pin, PORT_MODE_ANALOG, PORT_OE_IN);
        break;
    }
}

static uint16_t mdr_touch_read_wire_adc(void *user_data, rtouch_wire_t wire)
{
    (void)user_data;
    ADC1_SetChannel(mdr_touch_wire_channel(wire));
    ADC1_Start();
    while (ADC1_GetFlagStatus(TOUCH_ADC_FLAG) == RESET) {
    }
    return (uint16_t)(ADC1_GetResult() & 0x0FFFu);
}

static uint32_t mdr_touch_ps2_now_us(mdr_touch_ps2_mouse_t *ctx)
{
    uint16_t now = TIMER_GetCounter(TOUCH_TIMER);
    if (now < ctx->last_timer_us) {
        ctx->tick_hi_us += 0x10000u;
    }
    ctx->last_timer_us = now;
    return ctx->tick_hi_us + now;
}

static void mdr_touch_delay_us(void *user_data, uint32_t usec)
{
    mdr_touch_ps2_mouse_t *ctx = (mdr_touch_ps2_mouse_t *)user_data;
    uint32_t start = mdr_touch_ps2_now_us(ctx);
    while ((mdr_touch_ps2_now_us(ctx) - start) < usec) {
    }
}

static uint32_t mdr_touch_get_time_ms(void *user_data)
{
    return mdr_touch_ps2_now_us((mdr_touch_ps2_mouse_t *)user_data) / 1000u;
}

static void mdr_ps2_line_release(uint16_t pin)
{
    mdr_port_apply(MDR_PORTF, pin, PORT_MODE_DIGITAL, PORT_OE_IN);
}

static void mdr_ps2_line_low(uint16_t pin)
{
    mdr_port_apply(MDR_PORTF, pin, PORT_MODE_DIGITAL, PORT_OE_OUT);
    PORT_ResetBits(MDR_PORTF, pin);
}

static bool mdr_ps2_read(uint16_t pin)
{
    return PORT_ReadInputDataBit(MDR_PORTF, pin) != 0u;
}

static void mdr_ps2_send_bit(uint16_t data_pin, uint16_t clk_pin, bool bit_value)
{
    if (bit_value) {
        mdr_ps2_line_release(data_pin);
    } else {
        mdr_ps2_line_low(data_pin);
    }
    mdr_ps2_line_low(clk_pin);
    for (volatile uint32_t i = 0; i < 32u; ++i) {
    }
    mdr_ps2_line_release(clk_pin);
    for (volatile uint32_t i = 0; i < 32u; ++i) {
    }
}

static void mdr_ps2_begin_tx(mdr_touch_ps2_mouse_t *ctx)
{
    if (ctx->ps2_tx_active) {
        return;
    }
    if (!mdr_ps2_read(PS2_CLK_PIN) || !mdr_ps2_read(PS2_DATA_PIN)) {
        return;
    }
    if (!ps2_mouse_pop_tx_byte(&ctx->mouse, &ctx->ps2_tx_byte)) {
        return;
    }
    ctx->ps2_tx_bit = 0u;
    ctx->ps2_tx_active = true;
}

static void mdr_ps2_service_tx(mdr_touch_ps2_mouse_t *ctx)
{
    if (!ctx->ps2_tx_active) {
        mdr_ps2_begin_tx(ctx);
        return;
    }

    if (ctx->ps2_tx_bit == 0u) {
        mdr_ps2_line_low(PS2_DATA_PIN);
        mdr_touch_delay_us(ctx, 20u);
        mdr_ps2_line_release(PS2_CLK_PIN);
        ctx->ps2_tx_bit = 1u;
        return;
    }

    if ((ctx->ps2_tx_bit >= 1u) && (ctx->ps2_tx_bit <= 8u)) {
        bool data = ((ctx->ps2_tx_byte >> (ctx->ps2_tx_bit - 1u)) & 0x01u) != 0u;
        mdr_ps2_send_bit(PS2_DATA_PIN, PS2_CLK_PIN, data);
        ++ctx->ps2_tx_bit;
        return;
    }

    if (ctx->ps2_tx_bit == 9u) {
        uint8_t parity = 1u;
        for (uint8_t i = 0; i < 8u; ++i) {
            parity ^= (uint8_t)((ctx->ps2_tx_byte >> i) & 0x01u);
        }
        mdr_ps2_send_bit(PS2_DATA_PIN, PS2_CLK_PIN, parity != 0u);
        ++ctx->ps2_tx_bit;
        return;
    }

    if (ctx->ps2_tx_bit == 10u) {
        mdr_ps2_send_bit(PS2_DATA_PIN, PS2_CLK_PIN, true);
        mdr_ps2_line_release(PS2_DATA_PIN);
        ctx->ps2_tx_active = false;
    }
}

static void mdr_ps2_service_rx(mdr_touch_ps2_mouse_t *ctx)
{
    uint32_t now = mdr_touch_ps2_now_us(ctx);
    bool clk = mdr_ps2_read(PS2_CLK_PIN);
    bool data = mdr_ps2_read(PS2_DATA_PIN);

    if (!clk && data) {
        ctx->last_ps2_edge_us = now;
        ctx->ps2_rx_bit = 0u;
        ctx->ps2_rx_shift = 0u;
        ctx->ps2_rx_parity = 1u;
        return;
    }

    if ((ctx->last_ps2_edge_us != 0u) && ((now - ctx->last_ps2_edge_us) > 20000u)) {
        ctx->ps2_rx_bit = 0u;
    }

    if (!clk) {
        return;
    }

    if (ctx->ps2_rx_bit == 0u) {
        if (!data) {
            ctx->ps2_rx_bit = 1u;
            ctx->last_ps2_edge_us = now;
        }
        return;
    }

    if ((now - ctx->last_ps2_edge_us) < 30u) {
        return;
    }
    ctx->last_ps2_edge_us = now;

    if ((ctx->ps2_rx_bit >= 1u) && (ctx->ps2_rx_bit <= 8u)) {
        if (data) {
            ctx->ps2_rx_shift |= (uint8_t)(1u << (ctx->ps2_rx_bit - 1u));
            ctx->ps2_rx_parity ^= 1u;
        }
        ++ctx->ps2_rx_bit;
        return;
    }

    if (ctx->ps2_rx_bit == 9u) {
        if (data) {
            ctx->ps2_rx_parity ^= 1u;
        }
        ++ctx->ps2_rx_bit;
        return;
    }

    if (ctx->ps2_rx_bit == 10u) {
        if (data && (ctx->ps2_rx_parity != 0u)) {
            ps2_mouse_handle_host_byte(&ctx->mouse, ctx->ps2_rx_shift);
        }
        ctx->ps2_rx_bit = 0u;
    }
}

static void mdr_touch_ps2_apply_events(mdr_touch_ps2_mouse_t *ctx)
{
    const rtouch_state_t *state = rtouch_get_state(&ctx->touch);
    int16_t dx = state->dx / 256;
    int16_t dy = (int16_t)(-state->dy / 256);

    if ((dx != 0) || (dy != 0)) {
        ps2_mouse_move(&ctx->mouse, dx, dy);
    }

    if ((state->events & RTOUCH_EVENT_HOLD) != 0u) {
        ctx->hold_drag = true;
        ps2_mouse_set_buttons(&ctx->mouse, PS2_MOUSE_BTN_LEFT);
    }

    if (((state->events & RTOUCH_EVENT_TOUCH_UP) != 0u) && ctx->hold_drag) {
        ctx->hold_drag = false;
        ps2_mouse_set_buttons(&ctx->mouse, 0u);
    }

    if (((state->events & RTOUCH_EVENT_TAP) != 0u) && !ctx->hold_drag) {
        ps2_mouse_click(&ctx->mouse, PS2_MOUSE_BTN_LEFT, 1u);
    }

    if (((state->events & RTOUCH_EVENT_DOUBLE_TAP) != 0u) && !ctx->hold_drag) {
        ps2_mouse_click(&ctx->mouse, PS2_MOUSE_BTN_LEFT, 2u);
    }
}

void mdr_touch_ps2_mouse_init(mdr_touch_ps2_mouse_t *ctx)
{
    rtouch_port_t port;
    rtouch_config_t touch_cfg;
    ADC_InitTypeDef adc_cfg;
    ADCx_InitTypeDef adc1_cfg;
    TIMER_CntInitTypeDef timer_cfg;
    RST_CLK_FreqTypeDef freq;

    ps2_mouse_init(&ctx->mouse);
    rtouch_get_default_config(&touch_cfg);

    RST_CLK_PCLKcmd(RST_CLK_PCLK_PORTD | RST_CLK_PCLK_PORTF | RST_CLK_PCLK_ADC | RST_CLK_PCLK_TIMER1, ENABLE);

    RST_CLK_GetClocksFreq(&freq);

    ADC_StructInit(&adc_cfg);
    adc_cfg.ADC_SynchronousMode = ADC_SyncMode_Independent;
    ADC_Init(&adc_cfg);

    ADCx_StructInit(&adc1_cfg);
    adc1_cfg.ADC_ClockSource = ADC_CLOCK_SOURCE_CPU;
    adc1_cfg.ADC_SamplingMode = ADC_SAMPLING_MODE_SINGLE_CONV;
    adc1_cfg.ADC_ChannelSwitching = ADC_CH_SWITCHING_Disable;
    adc1_cfg.ADC_ChannelNumber = ADC_CH_ADC2;
    adc1_cfg.ADC_Channels = ADC_CH_ADC2_MSK;
    adc1_cfg.ADC_LevelControl = ADC_LEVEL_CONTROL_Disable;
    adc1_cfg.ADC_VRefSource = ADC_VREF_SOURCE_INTERNAL;
    adc1_cfg.ADC_IntVRefSource = ADC_INT_VREF_SOURCE_INEXACT;
    adc1_cfg.ADC_Prescaler = ADC_CLK_div_16;
    adc1_cfg.ADC_DelayGo = 0u;
    ADC1_Init(&adc1_cfg);
    ADC1_Cmd(ENABLE);

    TIMER_CntStructInit(&timer_cfg);
    TIMER_BRGInit(TOUCH_TIMER, TIMER_HCLKdiv1);
    timer_cfg.TIMER_Prescaler = (uint16_t)((freq.CPU_CLK_Frequency / 1000000u) - 1u);
    timer_cfg.TIMER_Period = 0xFFFFu;
    TIMER_CntInit(TOUCH_TIMER, &timer_cfg);
    TIMER_Cmd(TOUCH_TIMER, ENABLE);

    port.user_data = ctx;
    port.set_wire_mode = mdr_touch_set_wire_mode;
    port.read_wire_adc = mdr_touch_read_wire_adc;
    port.delay_us = mdr_touch_delay_us;
    port.get_time_ms = mdr_touch_get_time_ms;
    rtouch_init(&ctx->touch, &port, &touch_cfg);

    mdr_ps2_line_release(PS2_CLK_PIN);
    mdr_ps2_line_release(PS2_DATA_PIN);
    ctx->tick_hi_us = 0u;
    ctx->last_timer_us = 0u;
    ctx->last_ps2_edge_us = 0u;
    ctx->ps2_rx_bit = 0u;
    ctx->ps2_rx_shift = 0u;
    ctx->ps2_rx_parity = 1u;
    ctx->ps2_tx_active = false;
    ctx->hold_drag = false;
}

void mdr_touch_ps2_mouse_poll(mdr_touch_ps2_mouse_t *ctx)
{
    (void)mdr_touch_ps2_now_us(ctx);
    rtouch_update(&ctx->touch);
    mdr_touch_ps2_apply_events(ctx);
    mdr_ps2_service_rx(ctx);
    mdr_ps2_service_tx(ctx);
}
