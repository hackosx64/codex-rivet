#include "telemetry_log.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

#define TAG "telemetry_log"

static const char *k_log_path = APP_LOG_FILE_PATH;

esp_err_t telemetry_log_init(telemetry_log_t *log)
{
    memset(log, 0, sizeof(*log));
    log->mutex = xSemaphoreCreateMutex();
    if (log->mutex == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true,
    };

    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Не удалось смонтировать SPIFFS: %s", esp_err_to_name(err));
        return err;
    }
    log->mounted = true;
    return ESP_OK;
}

esp_err_t telemetry_log_start(telemetry_log_t *log, bool append)
{
    if (!log->mounted)
    {
        return ESP_FAIL;
    }

    if (xSemaphoreTake(log->mutex, pdMS_TO_TICKS(50)) != pdTRUE)
    {
        return ESP_ERR_TIMEOUT;
    }

    const char *mode = append ? "a" : "w";
    log->file = fopen(k_log_path, mode);
    if (log->file == NULL)
    {
        xSemaphoreGive(log->mutex);
        ESP_LOGE(TAG, "Не удалось открыть файл %s", k_log_path);
        return ESP_FAIL;
    }

    if (!append)
    {
        fprintf(log->file, "timestamp_ms,voltage_v,current_a,speed_rpm,temperature_c,motor_state,fault_mask,home,rew\n");
    }

    log->active = true;
    log->entries = 0;
    xSemaphoreGive(log->mutex);
    ESP_LOGI(TAG, "Логирование телеметрии запущено (%s)", append ? "продолжение" : "новый файл");
    return ESP_OK;
}

void telemetry_log_stop(telemetry_log_t *log)
{
    if (xSemaphoreTake(log->mutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        if (log->file != NULL)
        {
            fclose(log->file);
            log->file = NULL;
        }
        log->active = false;
        xSemaphoreGive(log->mutex);
    }
}

void telemetry_log_append(telemetry_log_t *log, const esc_telemetry_t *telemetry)
{
    if (!log->active || telemetry == NULL)
    {
        return;
    }
    if (xSemaphoreTake(log->mutex, pdMS_TO_TICKS(10)) != pdTRUE)
    {
        return;
    }
    if (log->file != NULL)
    {
        fprintf(log->file,
                "%u,%.2f,%.2f,%.1f,%.1f,%u,%u,%u,%u\n",
                telemetry->timestamp_ms,
                telemetry->voltage_v,
                telemetry->current_a,
                telemetry->speed_rpm,
                telemetry->temperature_c,
                telemetry->motor_state,
                telemetry->fault_mask,
                telemetry->home_state ? 1U : 0U,
                telemetry->rew_state ? 1U : 0U);
        fflush(log->file);
        log->entries++;
    }
    xSemaphoreGive(log->mutex);
}

bool telemetry_log_is_active(const telemetry_log_t *log)
{
    return log->active;
}

const char *telemetry_log_get_path(void)
{
    return k_log_path;
}
