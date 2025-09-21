#include "led_status.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

#define TAG "led"

static void led_apply_color(led_status_t *led, uint8_t r, uint8_t g, uint8_t b)
{
    if (led->strip == NULL)
    {
        return;
    }
    ESP_ERROR_CHECK(led_strip_set_pixel(led->strip, 0, r, g, b));
    ESP_ERROR_CHECK(led_strip_refresh(led->strip));
}

esp_err_t led_status_init(led_status_t *led, gpio_num_t pin)
{
    memset(led, 0, sizeof(*led));
    led->mode = LED_MODE_OFF;
    led->brightness = 32;
    led_strip_config_t strip_config = {
        .strip_gpio_num = pin,
        .max_leds = 1,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 0,
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &led->strip);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Не удалось инициализировать WS2812: %s", esp_err_to_name(err));
        led->strip = NULL;
        return err;
    }

    led_apply_color(led, 0, 0, 0);
    return ESP_OK;
}

void led_status_set_mode(led_status_t *led, led_mode_t mode)
{
    if (led->mode == mode)
    {
        return;
    }
    led->mode = mode;
    led->last_toggle_us = esp_timer_get_time();
    led->led_on = false;
    led_status_task(led);
}

void led_status_task(led_status_t *led)
{
    if (led->strip == NULL)
    {
        return;
    }

    uint64_t now = esp_timer_get_time();
    uint64_t interval_us = 0;
    uint8_t brightness = (uint8_t)led->brightness;

    switch (led->mode)
    {
    case LED_MODE_OFF:
        if (led->led_on)
        {
            led_apply_color(led, 0, 0, 0);
            led->led_on = false;
        }
        break;
    case LED_MODE_STEADY_GREEN:
        if (!led->led_on)
        {
            led_apply_color(led, 0, brightness, 0);
            led->led_on = true;
        }
        break;
    case LED_MODE_BLINK_SLOW:
        interval_us = 500000;
        if ((now - led->last_toggle_us) >= interval_us)
        {
            led->last_toggle_us = now;
            led->led_on = !led->led_on;
            led_apply_color(led, 0, led->led_on ? brightness : 0, 0);
        }
        break;
    case LED_MODE_BLINK_FAST:
        interval_us = 100000;
        if ((now - led->last_toggle_us) >= interval_us)
        {
            led->last_toggle_us = now;
            led->led_on = !led->led_on;
            led_apply_color(led, led->led_on ? brightness : 0, 0, 0);
        }
        break;
    case LED_MODE_WIFI_CONFIG:
        interval_us = 250000;
        if ((now - led->last_toggle_us) >= interval_us)
        {
            led->last_toggle_us = now;
            led->led_on = !led->led_on;
            if (led->led_on)
            {
                led_apply_color(led, 0, 0, brightness);
            }
            else
            {
                led_apply_color(led, 0, 0, 0);
            }
        }
        break;
    default:
        break;
    }
}
