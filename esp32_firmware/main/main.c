#include "app_config.h"
#include "button.h"
#include "led_status.h"
#include "state_machine.h"
#include "uart_comm.h"
#include "config_storage.h"
#include "telemetry_log.h"
#include "web_server.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "app";

static button_handle_t g_button;
static led_status_t g_led;
static app_config_t g_config;
static uart_comm_t g_uart;
static telemetry_log_t g_log;
static state_machine_t g_state;
static web_server_t g_web;

static void control_task(void *arg);

void app_main(void)
{
    ESP_LOGI(TAG, "Инициализация контроллера");

    ESP_ERROR_CHECK(config_storage_init());
    config_storage_load(&g_config);
    ESP_ERROR_CHECK(telemetry_log_init(&g_log));

    ESP_ERROR_CHECK(led_status_init(&g_led, APP_WS2812_PIN));
    button_init(&g_button, APP_BUTTON_PIN, APP_BUTTON_ACTIVE_LEVEL);

    gpio_config_t sensor_conf = {
        .pin_bit_mask = (1ULL << APP_SENSOR_HOME_PIN) | (1ULL << APP_SENSOR_REW_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = (APP_SENSOR_ACTIVE_LEVEL == 0) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = (APP_SENSOR_ACTIVE_LEVEL == 1) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&sensor_conf));

    ESP_ERROR_CHECK(uart_comm_init(&g_uart));
    uart_comm_start(&g_uart);

    state_machine_init(&g_state, &g_config, &g_uart, &g_log, &g_led);
    web_server_init(&g_web, &g_state, &g_config, &g_log, &g_uart);

    xTaskCreatePinnedToCore(control_task, "control", 8192, NULL, 5, NULL, tskNO_AFFINITY);
}

static void control_task(void *arg)
{
    TickType_t last_wake = xTaskGetTickCount();
    while (1)
    {
        TickType_t now = xTaskGetTickCount();
        button_event_t event = button_poll(&g_button, now);

        bool home_active = (gpio_get_level(APP_SENSOR_HOME_PIN) == APP_SENSOR_ACTIVE_LEVEL);
        bool rew_active = (gpio_get_level(APP_SENSOR_REW_PIN) == APP_SENSOR_ACTIVE_LEVEL);

        if (event == BUTTON_EVENT_TRIPLE_CLICK)
        {
            if (web_server_is_running(&g_web))
            {
                web_server_stop(&g_web);
                state_machine_set_wifi_mode(&g_state, false);
            }
            else
            {
                if (web_server_start_ap(&g_web) == ESP_OK)
                {
                    state_machine_set_wifi_mode(&g_state, true);
                }
            }
        }
        else if (event != BUTTON_EVENT_NONE)
        {
            state_machine_handle_button_event(&g_state, event, home_active, rew_active);
        }

        uart_comm_update_sensors(&g_uart, home_active, rew_active);

        esc_telemetry_t telemetry = {0};
        uart_comm_get_latest(&g_uart, &telemetry);
        bool telemetry_valid = uart_comm_is_active(&g_uart, now);
        bool fault_active = telemetry_valid && (telemetry.fault_mask != 0);
        state_machine_set_fault(&g_state, fault_active);

        state_machine_update(&g_state,
                             telemetry_valid ? &telemetry : NULL,
                             home_active,
                             rew_active,
                             button_is_pressed(&g_button),
                             now);

        led_status_task(&g_led);

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(APP_CONTROL_LOOP_PERIOD_MS));
    }
}
