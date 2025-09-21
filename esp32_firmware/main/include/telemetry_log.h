#ifndef TELEMETRY_LOG_H
#define TELEMETRY_LOG_H

#include "uart_comm.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdbool.h>
#include <stdio.h>

typedef struct
{
    bool              mounted;
    bool              active;
    FILE             *file;
    SemaphoreHandle_t mutex;
    size_t            entries;
} telemetry_log_t;

esp_err_t telemetry_log_init(telemetry_log_t *log);
esp_err_t telemetry_log_start(telemetry_log_t *log, bool append);
void telemetry_log_stop(telemetry_log_t *log);
void telemetry_log_append(telemetry_log_t *log, const esc_telemetry_t *telemetry);
bool telemetry_log_is_active(const telemetry_log_t *log);
const char *telemetry_log_get_path(void);

#endif /* TELEMETRY_LOG_H */
