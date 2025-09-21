#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "stm32g4xx_hal.h"
#include "app_config.h"
#include "fault_handler.h"

/* Предварительное объявление, чтобы избежать циклических зависимостей */
typedef enum MotorState_t MotorState_t;

/* Структура для хранения телеметрии */
typedef struct
{
    uint32_t timestamp_ms;   /* Метка времени измерений */
    float    dc_bus_voltage; /* Вольты */
    float    phase_current;  /* Амперы */
    float    speed_rpm;
    float    temperature_c;
    MotorState_t motor_state;
    bool     home_state;
    bool     rew_state;
} TelemetryData_t;

/* Контекст подсистемы телеметрии */
typedef struct
{
    ADC_HandleTypeDef   *hadc;
    UART_HandleTypeDef  *huart;
    TIM_HandleTypeDef   *htim;
    FaultContext_t      *fault_ctx;
    TelemetryData_t      latest;
    uint32_t             last_transmit_tick;
    uint16_t             sample_counter;
} TelemetryContext_t;

void Telemetry_Init(TelemetryContext_t *ctx,
                    ADC_HandleTypeDef *hadc,
                    UART_HandleTypeDef *huart,
                    TIM_HandleTypeDef *htim,
                    FaultContext_t *fault_ctx);
void Telemetry_Update(TelemetryContext_t *ctx, const FaultContext_t *fault_ctx, MotorState_t motor_state);
const TelemetryData_t *Telemetry_GetLatest(const TelemetryContext_t *ctx);
void Telemetry_TransmitIfDue(TelemetryContext_t *ctx);
void Telemetry_EncodeCSV(char *buffer, size_t buffer_size, const TelemetryData_t *data, const FaultContext_t *fault_ctx);
void Telemetry_UpdateContactStates(TelemetryContext_t *ctx, bool home, bool rew);

#endif /* TELEMETRY_H */
