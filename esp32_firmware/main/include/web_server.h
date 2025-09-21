#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "state_machine.h"
#include "uart_comm.h"
#include "telemetry_log.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include <stdbool.h>

typedef struct
{
    httpd_handle_t server;
    bool           running;
    state_machine_t *sm;
    app_config_t   *config;
    telemetry_log_t *log;
    uart_comm_t    *uart;
} web_server_t;

esp_err_t web_server_init(web_server_t *srv,
                          state_machine_t *sm,
                          app_config_t *config,
                          telemetry_log_t *log,
                          uart_comm_t *uart);
esp_err_t web_server_start_ap(web_server_t *srv);
void web_server_stop(web_server_t *srv);
bool web_server_is_running(const web_server_t *srv);

#endif /* WEB_SERVER_H */
