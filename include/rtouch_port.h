#ifndef RTOUCH_PORT_H
#define RTOUCH_PORT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RTOUCH_WIRE_XP = 0,
    RTOUCH_WIRE_XM = 1,
    RTOUCH_WIRE_YP = 2,
    RTOUCH_WIRE_YM = 3,
    RTOUCH_WIRE_COUNT = 4
} rtouch_wire_t;

typedef enum {
    RTOUCH_PIN_MODE_ANALOG = 0,
    RTOUCH_PIN_MODE_INPUT,
    RTOUCH_PIN_MODE_OUTPUT_LOW,
    RTOUCH_PIN_MODE_OUTPUT_HIGH
} rtouch_pin_mode_t;

typedef struct {
    void *user_data;
    void (*set_wire_mode)(void *user_data, rtouch_wire_t wire, rtouch_pin_mode_t mode);
    uint16_t (*read_wire_adc)(void *user_data, rtouch_wire_t wire);
    void (*delay_us)(void *user_data, uint32_t usec);
    uint32_t (*get_time_ms)(void *user_data);
} rtouch_port_t;

#ifdef __cplusplus
}
#endif

#endif
