#ifndef BUTTON_H
#define BUTTON_H

#include "driver/gpio.h"
#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"

typedef enum
{
    BUTTON_EVENT_NONE = 0,
    BUTTON_EVENT_PRESSED,
    BUTTON_EVENT_RELEASED,
    BUTTON_EVENT_TRIPLE_CLICK
} button_event_t;

typedef struct
{
    gpio_num_t pin;
    uint8_t    active_level;
    bool       state;
    bool       last_state;
    TickType_t last_change_tick;
    TickType_t press_tick;
    TickType_t last_release_tick;
    uint8_t    click_count;
} button_handle_t;

void button_init(button_handle_t *btn, gpio_num_t pin, uint8_t active_level);
button_event_t button_poll(button_handle_t *btn, TickType_t now);
bool button_is_pressed(const button_handle_t *btn);

#endif /* BUTTON_H */
