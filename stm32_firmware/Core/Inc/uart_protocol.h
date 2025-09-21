#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "stm32g4xx_hal.h"
#include "motor_control.h"
#include "telemetry.h"
#include "fault_handler.h"

typedef struct
{
    UART_HandleTypeDef      *huart;
    MotorControlContext_t   *motor_ctx;
    TelemetryContext_t      *telemetry_ctx;
    FaultContext_t          *fault_ctx;
    char                     rx_buffer[APP_UART_RX_BUFFER_SIZE];
    size_t                   rx_index;
} UARTProtocolContext_t;

void UARTProtocol_Init(UARTProtocolContext_t *ctx,
                       UART_HandleTypeDef *huart,
                       MotorControlContext_t *motor_ctx,
                       TelemetryContext_t *telemetry_ctx,
                       FaultContext_t *fault_ctx);
void UARTProtocol_Process(UARTProtocolContext_t *ctx);
void UARTProtocol_SendStatus(UARTProtocolContext_t *ctx);

#endif /* UART_PROTOCOL_H */
