#include "uart_comm.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include <string.h>
#include <stdlib.h>

#define TAG "uart_comm"
#define UART_BUFFER_SIZE 256
#define SENSOR_SYNC_INTERVAL_MS 100

static void uart_rx_task(void *arg);
static esp_err_t uart_send_raw(const char *cmd);
static void parse_csv(uart_comm_t *ctx, char *line);

static uart_comm_t *s_uart_ctx = NULL;

esp_err_t uart_comm_init(uart_comm_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->mutex = xSemaphoreCreateMutex();
    if (ctx->mutex == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    uart_config_t uart_config = {
        .baud_rate = APP_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    ESP_ERROR_CHECK(uart_param_config(APP_UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(APP_UART_PORT, APP_UART_TX_PIN, APP_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(APP_UART_PORT, UART_BUFFER_SIZE * 2, 0, 0, NULL, 0));

    s_uart_ctx = ctx;
    return ESP_OK;
}

void uart_comm_start(uart_comm_t *ctx)
{
    xTaskCreatePinnedToCore(uart_rx_task, "uart_rx", 4096, ctx, 4, NULL, tskNO_AFFINITY);
}

void uart_comm_get_latest(uart_comm_t *ctx, esc_telemetry_t *out)
{
    if (ctx == NULL || out == NULL)
    {
        return;
    }
    if (xSemaphoreTake(ctx->mutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        *out = ctx->latest;
        xSemaphoreGive(ctx->mutex);
    }
}

void uart_comm_update_sensors(uart_comm_t *ctx, bool home, bool rew)
{
    if (ctx == NULL)
    {
        return;
    }
    TickType_t now = xTaskGetTickCount();
    bool changed = (ctx->sensors_home != home) || (ctx->sensors_rew != rew);
    if (changed || (now - ctx->last_sensor_sync_tick) > pdMS_TO_TICKS(SENSOR_SYNC_INTERVAL_MS))
    {
        ctx->sensors_home = home;
        ctx->sensors_rew = rew;
        ctx->last_sensor_sync_tick = now;
        char cmd[32];
        snprintf(cmd, sizeof(cmd), "SENSORS:%d,%d\r\n", home ? 1 : 0, rew ? 1 : 0);
        uart_send_raw(cmd);
    }
}

esp_err_t uart_comm_send_start(uart_comm_t *ctx, bool forward)
{
    (void)ctx;
    return uart_send_raw(forward ? "START:FWD\r\n" : "START:REV\r\n");
}

esp_err_t uart_comm_send_stop(uart_comm_t *ctx)
{
    (void)ctx;
    return uart_send_raw("STOP\r\n");
}

esp_err_t uart_comm_send_reverse(uart_comm_t *ctx)
{
    (void)ctx;
    return uart_send_raw("REV\r\n");
}

esp_err_t uart_comm_send_speed(uart_comm_t *ctx, uint32_t rpm)
{
    (void)ctx;
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "SPEED:%u\r\n", rpm);
    return uart_send_raw(cmd);
}

esp_err_t uart_comm_send_calibrate(uart_comm_t *ctx)
{
    (void)ctx;
    return uart_send_raw("CALIBRATE\r\n");
}

bool uart_comm_is_active(const uart_comm_t *ctx, TickType_t now)
{
    if (ctx == NULL)
    {
        return false;
    }
    return ctx->telemetry_valid && (now - ctx->last_update_tick) < pdMS_TO_TICKS(APP_TELEMETRY_TIMEOUT_MS);
}

static void uart_rx_task(void *arg)
{
    uart_comm_t *ctx = (uart_comm_t *)arg;
    uint8_t data;
    char line[UART_BUFFER_SIZE];
    size_t index = 0;

    while (1)
    {
        int len = uart_read_bytes(APP_UART_PORT, &data, 1, pdMS_TO_TICKS(20));
        if (len == 1)
        {
            if (data == '\r')
            {
                continue;
            }
            if (data == '\n')
            {
                if (index > 0)
                {
                    line[index] = '\0';
                    parse_csv(ctx, line);
                    index = 0;
                }
            }
            else if (index < (sizeof(line) - 1))
            {
                line[index++] = (char)data;
            }
        }
    }
}

static void parse_csv(uart_comm_t *ctx, char *line)
{
    esc_telemetry_t tmp = {0};
    char *token = strtok(line, ",");
    int field = 0;
    while (token != NULL)
    {
        switch (field)
        {
        case 0: tmp.voltage_v = strtof(token, NULL); break;
        case 1: tmp.current_a = strtof(token, NULL); break;
        case 2: tmp.speed_rpm = strtof(token, NULL); break;
        case 3: tmp.temperature_c = strtof(token, NULL); break;
        case 4: tmp.motor_state = (uint32_t)strtoul(token, NULL, 10); break;
        case 5: tmp.fault_mask = (uint32_t)strtoul(token, NULL, 10); break;
        case 6: tmp.home_state = (strtoul(token, NULL, 10) != 0); break;
        case 7: tmp.rew_state = (strtoul(token, NULL, 10) != 0); break;
        default: break;
        }
        token = strtok(NULL, ",");
        field++;
    }
    tmp.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);

    if (xSemaphoreTake(ctx->mutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        ctx->latest = tmp;
        ctx->telemetry_valid = true;
        ctx->last_update_tick = xTaskGetTickCount();
        xSemaphoreGive(ctx->mutex);
    }
}

static esp_err_t uart_send_raw(const char *cmd)
{
    if (cmd == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    int len = strlen(cmd);
    int written = uart_write_bytes(APP_UART_PORT, cmd, len);
    if (written != len)
    {
        ESP_LOGW(TAG, "Не удалось отправить всю команду (%d/%d)", written, len);
        return ESP_FAIL;
    }
    return ESP_OK;
}
