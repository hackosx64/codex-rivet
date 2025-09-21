#include "app_config.h"

void app_config_set_defaults(app_config_t *cfg)
{
    cfg->target_rpm = APP_MAX_RPM;
    cfg->ramp_up_ms = 500;
    cfg->ramp_down_ms = 200;
    cfg->high_load_current_a = 30.0f;
    cfg->low_load_current_a = 5.0f;
    cfg->load_drop_delay_ms = 150;
    cfg->settle_delay_ms = 500;
    cfg->logging_enabled = false;
}
