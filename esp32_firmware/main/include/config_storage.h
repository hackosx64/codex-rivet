#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include "app_config.h"
#include "esp_err.h"

esp_err_t config_storage_init(void);
esp_err_t config_storage_load(app_config_t *cfg);
esp_err_t config_storage_save(const app_config_t *cfg);

#endif /* CONFIG_STORAGE_H */
