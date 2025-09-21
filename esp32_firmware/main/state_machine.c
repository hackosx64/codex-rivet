#include "state_machine.h"
#include "esp_log.h"
#include "esp_err.h"
#include <math.h>

#define TAG "state"
#define SPEED_STABLE_RPM 200.0f
#define SPEED_STOPPED_RPM 80.0f

static void update_led(state_machine_t *sm);
static void start_motor(state_machine_t *sm, bool forward, TickType_t now);
static void request_stop(state_machine_t *sm, TickType_t now);
static void request_reverse(state_machine_t *sm, bool forward, TickType_t now);

void state_machine_init(state_machine_t *sm,
                        app_config_t *config,
                        uart_comm_t *uart,
                        telemetry_log_t *log,
                        led_status_t *led)
{
    sm->config = config;
    sm->uart = uart;
    sm->log = log;
    sm->led = led;
    sm->state = APP_STATE_IDLE;
    sm->wifi_mode = false;
    sm->fault_active = false;
    sm->button_held = false;
    sm->current_direction_forward = true;
    sm->reverse_after_stop = false;
    sm->next_direction_forward = true;
    sm->state_enter_tick = xTaskGetTickCount();
    sm->load_high_tick = 0;
    sm->load_drop_tick = 0;
    sm->load_detected = false;
    update_led(sm);
}

void state_machine_handle_button_event(state_machine_t *sm,
                                       button_event_t event,
                                       bool home_active,
                                       bool rew_active)
{
    TickType_t now = xTaskGetTickCount();
    switch (event)
    {
    case BUTTON_EVENT_PRESSED:
        sm->button_held = true;
        if (sm->state == APP_STATE_IDLE)
        {
            bool forward = true;
            if (rew_active && !home_active)
            {
                forward = false;
            }
            start_motor(sm, forward, now);
        }
        break;
    case BUTTON_EVENT_RELEASED:
        sm->button_held = false;
        if (sm->state == APP_STATE_RUNNING_FORWARD && !rew_active && !sm->reverse_after_stop)
        {
            request_reverse(sm, false, now);
        }
        break;
    case BUTTON_EVENT_TRIPLE_CLICK:
    default:
        break;
    }
}

void state_machine_update(state_machine_t *sm,
                          const esc_telemetry_t *telemetry,
                          bool home_active,
                          bool rew_active,
                          bool button_pressed,
                          TickType_t now)
{
    sm->button_held = button_pressed;

    if (sm->config->logging_enabled)
    {
        if (!telemetry_log_is_active(sm->log))
        {
            esp_err_t err = telemetry_log_start(sm->log, false);
            if (err != ESP_OK)
            {
                ESP_LOGW(TAG, "Не удалось запустить логирование: %s", esp_err_to_name(err));
            }
        }
    }
    else if (telemetry_log_is_active(sm->log))
    {
        telemetry_log_stop(sm->log);
    }

    if (telemetry_log_is_active(sm->log) && telemetry != NULL)
    {
        telemetry_log_append(sm->log, telemetry);
    }

    switch (sm->state)
    {
    case APP_STATE_IDLE:
        break;

    case APP_STATE_STARTING_FORWARD:
        if (telemetry != NULL && fabsf(telemetry->speed_rpm) > SPEED_STABLE_RPM)
        {
            sm->state = APP_STATE_RUNNING_FORWARD;
            sm->state_enter_tick = now;
            sm->load_detected = false;
            sm->load_high_tick = now;
            ESP_LOGI(TAG, "Двигатель вышел на обороты (вперёд)");
        }
        break;

    case APP_STATE_RUNNING_FORWARD:
        if (telemetry != NULL)
        {
            if (telemetry->current_a >= sm->config->high_load_current_a)
            {
                sm->load_detected = true;
                sm->load_high_tick = now;
            }
        if (sm->load_detected && telemetry->current_a <= sm->config->low_load_current_a)
        {
            if ((now - sm->load_high_tick) >= pdMS_TO_TICKS(sm->config->load_drop_delay_ms) && !sm->reverse_after_stop)
            {
                ESP_LOGI(TAG, "Обнаружено падение нагрузки, инициируем реверс");
                request_reverse(sm, false, now);
            }
        }
    }

        if (rew_active && !sm->reverse_after_stop)
        {
            ESP_LOGI(TAG, "Достигнута точка REW, инициируем реверс");
            request_reverse(sm, false, now);
        }
        break;

    case APP_STATE_STARTING_REVERSE:
        if (telemetry != NULL && fabsf(telemetry->speed_rpm) > SPEED_STABLE_RPM)
        {
            sm->state = APP_STATE_RUNNING_REVERSE;
            sm->state_enter_tick = now;
            ESP_LOGI(TAG, "Двигатель вышел на обороты (реверс)");
        }
        break;

    case APP_STATE_RUNNING_REVERSE:
        if (home_active)
        {
            ESP_LOGI(TAG, "Достигнута HOME, останавливаемся");
            request_stop(sm, now);
            sm->reverse_after_stop = false;
        }
        break;

    case APP_STATE_BRAKING:
        if (telemetry != NULL)
        {
            if (fabsf(telemetry->speed_rpm) < SPEED_STOPPED_RPM)
            {
                if ((now - sm->state_enter_tick) >= pdMS_TO_TICKS(sm->config->settle_delay_ms))
                {
                    if (sm->reverse_after_stop)
                    {
                        bool next_forward = sm->next_direction_forward;
                        sm->reverse_after_stop = false;
                        start_motor(sm, next_forward, now);
                    }
                    else
                    {
                        sm->state = APP_STATE_IDLE;
                        sm->state_enter_tick = now;
                        update_led(sm);
                    }
                }
            }
        }
        break;

    case APP_STATE_FAULT:
        /* Ожидаем снятия аварии */
        break;
    }

    update_led(sm);
}

void state_machine_set_fault(state_machine_t *sm, bool fault_active)
{
    if (sm->fault_active == fault_active)
    {
        return;
    }
    sm->fault_active = fault_active;
    if (fault_active)
    {
        ESP_LOGE(TAG, "Авария, останов");
        request_stop(sm, xTaskGetTickCount());
        sm->state = APP_STATE_FAULT;
    }
    else
    {
        ESP_LOGI(TAG, "Авария снята");
        sm->state = APP_STATE_IDLE;
    }
    update_led(sm);
}

void state_machine_set_wifi_mode(state_machine_t *sm, bool enabled)
{
    sm->wifi_mode = enabled;
    update_led(sm);
}

app_state_t state_machine_get_state(const state_machine_t *sm)
{
    return sm->state;
}

bool state_machine_is_wifi_mode(const state_machine_t *sm)
{
    return sm->wifi_mode;
}

void state_machine_command_start(state_machine_t *sm, bool forward)
{
    TickType_t now = xTaskGetTickCount();
    start_motor(sm, forward, now);
}

void state_machine_command_stop(state_machine_t *sm)
{
    TickType_t now = xTaskGetTickCount();
    sm->reverse_after_stop = false;
    request_stop(sm, now);
}

void state_machine_command_reverse(state_machine_t *sm)
{
    TickType_t now = xTaskGetTickCount();
    request_reverse(sm, !sm->current_direction_forward, now);
}

static void start_motor(state_machine_t *sm, bool forward, TickType_t now)
{
    sm->current_direction_forward = forward;
    sm->state_enter_tick = now;
    sm->load_detected = false;
    sm->load_high_tick = now;
    sm->load_drop_tick = now;
    sm->reverse_after_stop = false;
    sm->next_direction_forward = forward;

    uart_comm_send_speed(sm->uart, sm->config->target_rpm);
    uart_comm_send_start(sm->uart, forward);
    sm->state = forward ? APP_STATE_STARTING_FORWARD : APP_STATE_STARTING_REVERSE;
    ESP_LOGI(TAG, "Старт двигателя (%s)", forward ? "прямое" : "реверс");
    update_led(sm);
}

static void request_stop(state_machine_t *sm, TickType_t now)
{
    if (sm->state == APP_STATE_BRAKING)
    {
        return;
    }
    uart_comm_send_stop(sm->uart);
    sm->state = APP_STATE_BRAKING;
    sm->state_enter_tick = now;
    ESP_LOGI(TAG, "Принудительное торможение");
    update_led(sm);
}

static void request_reverse(state_machine_t *sm, bool forward, TickType_t now)
{
    sm->reverse_after_stop = true;
    sm->next_direction_forward = forward;
    request_stop(sm, now);
}

static void update_led(state_machine_t *sm)
{
    if (sm->fault_active)
    {
        led_status_set_mode(sm->led, LED_MODE_BLINK_FAST);
    }
    else if (sm->wifi_mode)
    {
        led_status_set_mode(sm->led, LED_MODE_WIFI_CONFIG);
    }
    else if (sm->state == APP_STATE_IDLE)
    {
        led_status_set_mode(sm->led, LED_MODE_STEADY_GREEN);
    }
    else
    {
        led_status_set_mode(sm->led, LED_MODE_STEADY_GREEN);
    }
}
