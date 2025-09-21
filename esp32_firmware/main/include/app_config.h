#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "driver/uart.h"
#include "driver/gpio.h"

#include <stdbool.h>
#include <stdint.h>

#define APP_UART_PORT              UART_NUM_1
#define APP_UART_TX_PIN            GPIO_NUM_43
#define APP_UART_RX_PIN            GPIO_NUM_44
#define APP_UART_BAUD_RATE         115200

#define APP_BUTTON_PIN             GPIO_NUM_47
#define APP_SENSOR_HOME_PIN        GPIO_NUM_17
#define APP_SENSOR_REW_PIN         GPIO_NUM_18
#define APP_WS2812_PIN             GPIO_NUM_46

#define APP_BUTTON_ACTIVE_LEVEL    0
#define APP_SENSOR_ACTIVE_LEVEL    0

#define APP_WIFI_AP_SSID           "BLDC-Setup"
#define APP_WIFI_AP_PASSWORD       "bldc-config"
#define APP_WIFI_CHANNEL           6

#define APP_MAX_RPM                50000
#define APP_CONTROL_LOOP_PERIOD_MS 10
#define APP_TELEMETRY_TIMEOUT_MS   500

#define APP_LOG_FILE_PATH          "/spiffs/session.csv"

typedef struct
{
    uint32_t target_rpm;
    uint32_t ramp_up_ms;
    uint32_t ramp_down_ms;
    float    high_load_current_a;
    float    low_load_current_a;
    uint32_t load_drop_delay_ms;
    uint32_t settle_delay_ms;
    bool     logging_enabled;
} app_config_t;

void app_config_set_defaults(app_config_t *cfg);

#endif /* APP_CONFIG_H */
