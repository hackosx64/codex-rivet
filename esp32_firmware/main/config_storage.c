#include "config_storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

#define TAG "config"
#define NVS_NAMESPACE "bldc"
#define NVS_KEY "params"

esp_err_t config_storage_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

esp_err_t config_storage_load(app_config_t *cfg)
{
    app_config_set_defaults(cfg);
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Используются настройки по умолчанию (%s)", esp_err_to_name(err));
        return err;
    }

    size_t required_size = sizeof(*cfg);
    err = nvs_get_blob(handle, NVS_KEY, cfg, &required_size);
    nvs_close(handle);

    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Не удалось прочитать конфигурацию (%s)", esp_err_to_name(err));
        app_config_set_defaults(cfg);
    }
    return err;
}

esp_err_t config_storage_save(const app_config_t *cfg)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_set_blob(handle, NVS_KEY, cfg, sizeof(*cfg));
    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Конфигурация сохранена");
    }
    else
    {
        ESP_LOGE(TAG, "Ошибка сохранения конфигурации: %s", esp_err_to_name(err));
    }
    return err;
}
