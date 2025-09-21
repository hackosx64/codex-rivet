#ifndef LED_STATUS_H
#define LED_STATUS_H

#include "driver/gpio.h"
#include "led_strip.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    LED_MODE_OFF = 0,
    LED_MODE_STEADY_GREEN,
    LED_MODE_BLINK_SLOW,
    LED_MODE_BLINK_FAST,
    LED_MODE_WIFI_CONFIG
} led_mode_t;

typedef struct
{
    led_strip_handle_t strip;
    led_mode_t         mode;
    int                brightness;
    uint64_t           last_toggle_us;
    bool               led_on;
} led_status_t;

esp_err_t led_status_init(led_status_t *led, gpio_num_t pin);
void led_status_set_mode(led_status_t *led, led_mode_t mode);
void led_status_task(led_status_t *led);

#endif /* LED_STATUS_H */
