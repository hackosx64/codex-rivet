#ifndef UART_COMM_H
#define UART_COMM_H

#include "app_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    float    voltage_v;
    float    current_a;
    float    speed_rpm;
    float    temperature_c;
    uint32_t motor_state;
    uint32_t fault_mask;
    bool     home_state;
    bool     rew_state;
    uint32_t timestamp_ms;
} esc_telemetry_t;

typedef struct
{
    SemaphoreHandle_t mutex;
    esc_telemetry_t   latest;
    bool              telemetry_valid;
    TickType_t        last_update_tick;
    bool              sensors_home;
    bool              sensors_rew;
    TickType_t        last_sensor_sync_tick;
} uart_comm_t;

esp_err_t uart_comm_init(uart_comm_t *ctx);
void uart_comm_start(uart_comm_t *ctx);
void uart_comm_get_latest(uart_comm_t *ctx, esc_telemetry_t *out);
void uart_comm_update_sensors(uart_comm_t *ctx, bool home, bool rew);

esp_err_t uart_comm_send_start(uart_comm_t *ctx, bool forward);
esp_err_t uart_comm_send_stop(uart_comm_t *ctx);
esp_err_t uart_comm_send_reverse(uart_comm_t *ctx);
esp_err_t uart_comm_send_speed(uart_comm_t *ctx, uint32_t rpm);
esp_err_t uart_comm_send_calibrate(uart_comm_t *ctx);
bool uart_comm_is_active(const uart_comm_t *ctx, TickType_t now);

#endif /* UART_COMM_H */
