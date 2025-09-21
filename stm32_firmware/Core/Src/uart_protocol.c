#include "uart_protocol.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

static void process_line(UARTProtocolContext_t *ctx, const char *line);
static void send_response(UARTProtocolContext_t *ctx, const char *text);
static int stricmp_local(const char *a, const char *b);
static int strnicmp_local(const char *a, const char *b, size_t n);

void UARTProtocol_Init(UARTProtocolContext_t *ctx,
                       UART_HandleTypeDef *huart,
                       MotorControlContext_t *motor_ctx,
                       TelemetryContext_t *telemetry_ctx,
                       FaultContext_t *fault_ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->huart          = huart;
    ctx->motor_ctx      = motor_ctx;
    ctx->telemetry_ctx  = telemetry_ctx;
    ctx->fault_ctx      = fault_ctx;
}

void UARTProtocol_Process(UARTProtocolContext_t *ctx)
{
    uint8_t byte;
    while (HAL_UART_Receive(ctx->huart, &byte, 1U, 0U) == HAL_OK)
    {
        if (byte == '\r' || byte == '\n')
        {
            if (ctx->rx_index > 0U)
            {
                ctx->rx_buffer[ctx->rx_index] = '\0';
                process_line(ctx, ctx->rx_buffer);
                ctx->rx_index = 0U;
            }
        }
        else if (ctx->rx_index < (sizeof(ctx->rx_buffer) - 1U))
        {
            ctx->rx_buffer[ctx->rx_index++] = (char)byte;
        }
    }
}

void UARTProtocol_SendStatus(UARTProtocolContext_t *ctx)
{
    char buffer[APP_UART_TX_BUFFER_SIZE];
    Telemetry_EncodeCSV(buffer,
                        sizeof(buffer),
                        Telemetry_GetLatest(ctx->telemetry_ctx),
                        ctx->fault_ctx);
    send_response(ctx, buffer);
}

static void process_line(UARTProtocolContext_t *ctx, const char *line)
{
    if (stricmp_local(line, "START") == 0)
    {
        MotorControl_RequestStart(ctx->motor_ctx, MOTOR_DIRECTION_FORWARD);
        send_response(ctx, "OK\r\n");
    }
    else if (strnicmp_local(line, "START:", 6) == 0)
    {
        const char *direction = line + 6;
        if (stricmp_local(direction, "FWD") == 0)
        {
            MotorControl_RequestStart(ctx->motor_ctx, MOTOR_DIRECTION_FORWARD);
            send_response(ctx, "OK\r\n");
        }
        else if (stricmp_local(direction, "REV") == 0)
        {
            MotorControl_RequestStart(ctx->motor_ctx, MOTOR_DIRECTION_REVERSE);
            send_response(ctx, "OK\r\n");
        }
        else
        {
            send_response(ctx, "ERR:BAD_DIR\r\n");
        }
    }
    else if (stricmp_local(line, "STOP") == 0)
    {
        MotorControl_RequestStop(ctx->motor_ctx);
        send_response(ctx, "OK\r\n");
    }
    else if (stricmp_local(line, "REV") == 0)
    {
        MotorControl_RequestReverse(ctx->motor_ctx);
        send_response(ctx, "OK\r\n");
    }
    else if (strnicmp_local(line, "SPEED:", 6) == 0)
    {
        uint32_t rpm = (uint32_t)strtoul(line + 6, NULL, 10);
        MotorControl_RequestSpeed(ctx->motor_ctx, rpm);
        send_response(ctx, "OK\r\n");
    }
    else if (stricmp_local(line, "CALIBRATE") == 0)
    {
        MotorControl_RequestCalibrate(ctx->motor_ctx);
        send_response(ctx, "OK\r\n");
    }
    else if (strnicmp_local(line, "SENSORS:", 8) == 0)
    {
        int home = 0;
        int rew  = 0;
        sscanf(line + 8, "%d,%d", &home, &rew);
        Telemetry_UpdateContactStates(ctx->telemetry_ctx, home != 0, rew != 0);
        send_response(ctx, "OK\r\n");
    }
    else if (stricmp_local(line, "STATUS") == 0)
    {
        UARTProtocol_SendStatus(ctx);
    }
    else
    {
        send_response(ctx, "ERR:UNKNOWN\r\n");
    }
}

static void send_response(UARTProtocolContext_t *ctx, const char *text)
{
    HAL_UART_Transmit(ctx->huart, (const uint8_t *)text, strlen(text), HAL_MAX_DELAY);
}

static int stricmp_local(const char *a, const char *b)
{
    return strnicmp_local(a, b, SIZE_MAX);
}

static int strnicmp_local(const char *a, const char *b, size_t n)
{
    size_t i = 0U;
    while (i < n && a[i] != '\0' && b[i] != '\0')
    {
        int ca = tolower((unsigned char)a[i]);
        int cb = tolower((unsigned char)b[i]);
        if (ca != cb)
        {
            return ca - cb;
        }
        ++i;
    }
    if (i == n)
    {
        return 0;
    }
    return tolower((unsigned char)a[i]) - tolower((unsigned char)b[i]);
}
