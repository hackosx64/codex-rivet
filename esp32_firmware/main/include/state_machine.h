#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "app_config.h"
#include "uart_comm.h"
#include "telemetry_log.h"
#include "led_status.h"
#include "button.h"
#include "esp_err.h"
#include <stdbool.h>

typedef enum
{
    APP_STATE_IDLE = 0,
    APP_STATE_STARTING_FORWARD,
    APP_STATE_RUNNING_FORWARD,
    APP_STATE_STARTING_REVERSE,
    APP_STATE_RUNNING_REVERSE,
    APP_STATE_BRAKING,
    APP_STATE_FAULT
} app_state_t;

typedef struct
{
    app_config_t   *config;
    uart_comm_t    *uart;
    telemetry_log_t *log;
    led_status_t   *led;

    app_state_t     state;
    bool            wifi_mode;
    bool            fault_active;
    bool            button_held;
    bool            current_direction_forward;
    bool            reverse_after_stop;
    bool            next_direction_forward;
    TickType_t      state_enter_tick;
    TickType_t      load_high_tick;
    TickType_t      load_drop_tick;
    bool            load_detected;
} state_machine_t;

void state_machine_init(state_machine_t *sm,
                        app_config_t *config,
                        uart_comm_t *uart,
                        telemetry_log_t *log,
                        led_status_t *led);
void state_machine_handle_button_event(state_machine_t *sm,
                                       button_event_t event,
                                       bool home_active,
                                       bool rew_active);
void state_machine_update(state_machine_t *sm,
                          const esc_telemetry_t *telemetry,
                          bool home_active,
                          bool rew_active,
                          bool button_pressed,
                          TickType_t now);
void state_machine_set_fault(state_machine_t *sm, bool fault_active);
void state_machine_set_wifi_mode(state_machine_t *sm, bool enabled);
app_state_t state_machine_get_state(const state_machine_t *sm);
bool state_machine_is_wifi_mode(const state_machine_t *sm);
void state_machine_command_start(state_machine_t *sm, bool forward);
void state_machine_command_stop(state_machine_t *sm);
void state_machine_command_reverse(state_machine_t *sm);

#endif /* STATE_MACHINE_H */
