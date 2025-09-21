#include "button.h"
#include "esp_log.h"

#define TAG "button"
#define DEBOUNCE_MS          30
#define CLICK_SEQUENCE_MS    1200

void button_init(button_handle_t *btn, gpio_num_t pin, uint8_t active_level)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = (active_level == 0) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = (active_level == 1) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    btn->pin = pin;
    btn->active_level = active_level;
    btn->state = (gpio_get_level(pin) == active_level);
    btn->last_state = btn->state;
    btn->last_change_tick = xTaskGetTickCount();
    btn->press_tick = btn->last_change_tick;
    btn->last_release_tick = 0;
    btn->click_count = 0;
}

button_event_t button_poll(button_handle_t *btn, TickType_t now)
{
    bool level = (gpio_get_level(btn->pin) == btn->active_level);
    if (level != btn->state)
    {
        if ((now - btn->last_change_tick) >= pdMS_TO_TICKS(DEBOUNCE_MS))
        {
            btn->last_state = btn->state;
            btn->state = level;
            btn->last_change_tick = now;

            if (btn->state)
            {
                btn->press_tick = now;
                return BUTTON_EVENT_PRESSED;
            }
            else
            {
                if ((now - btn->last_release_tick) > pdMS_TO_TICKS(CLICK_SEQUENCE_MS))
                {
                    btn->click_count = 0;
                }
                btn->last_release_tick = now;
                btn->click_count++;
                if (btn->click_count >= 3)
                {
                    btn->click_count = 0;
                    return BUTTON_EVENT_TRIPLE_CLICK;
                }
                return BUTTON_EVENT_RELEASED;
            }
        }
    }
    return BUTTON_EVENT_NONE;
}

bool button_is_pressed(const button_handle_t *btn)
{
    return btn->state;
}
