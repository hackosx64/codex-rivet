#include "telemetry.h"
#include "motor_control.h"
#include <stdio.h>
#include <string.h>

/* Прототипы функций библиотеки X-CUBE-MCSDK */
extern int16_t MC_GetMecSpeedAverageMotor1(void);
extern uint16_t MC_GetCalibratedVbusVoltageMotor1(void);
extern int16_t MC_GetPhaseCurrentAmplitudeMotor1(void);
extern int16_t MC_GetAvTempMotor1(void);

static float convert_voltage(uint16_t raw_mv);
static float convert_current(int16_t raw_ma);
static float convert_temperature(int16_t raw_decicelsius);

void Telemetry_Init(TelemetryContext_t *ctx,
                    ADC_HandleTypeDef *hadc,
                    UART_HandleTypeDef *huart,
                    TIM_HandleTypeDef *htim,
                    FaultContext_t *fault_ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->hadc  = hadc;
    ctx->huart = huart;
    ctx->htim  = htim;
    ctx->fault_ctx = fault_ctx;
    ctx->latest.home_state = false;
    ctx->latest.rew_state  = false;
}

void Telemetry_Update(TelemetryContext_t *ctx, const FaultContext_t *fault_ctx, MotorState_t motor_state)
{
    ctx->latest.timestamp_ms = HAL_GetTick();
    ctx->latest.motor_state  = motor_state;

    /* Напряжение шины */
    ctx->latest.dc_bus_voltage = convert_voltage(MC_GetCalibratedVbusVoltageMotor1());

    /* Ток фазы */
    ctx->latest.phase_current = convert_current(MC_GetPhaseCurrentAmplitudeMotor1());

    /* Скорость */
    ctx->latest.speed_rpm = (float)MC_GetMecSpeedAverageMotor1();

    /* Температура силовой части */
    ctx->latest.temperature_c = convert_temperature(MC_GetAvTempMotor1());

    /* Если активна критическая ошибка, выставим скорость в 0 для логики ESP32 */
    if (FaultHandler_HasCriticalFault(fault_ctx))
    {
        ctx->latest.speed_rpm = 0.0f;
    }
}

const TelemetryData_t *Telemetry_GetLatest(const TelemetryContext_t *ctx)
{
    return &ctx->latest;
}

void Telemetry_TransmitIfDue(TelemetryContext_t *ctx)
{
    uint32_t now = HAL_GetTick();
    if ((now - ctx->last_transmit_tick) < APP_TELEMETRY_PERIOD_MS)
    {
        return;
    }

    ctx->last_transmit_tick = now;

    char buffer[APP_UART_TX_BUFFER_SIZE];
    Telemetry_EncodeCSV(buffer, sizeof(buffer), &ctx->latest, ctx->fault_ctx);
    HAL_UART_Transmit(ctx->huart, (uint8_t *)buffer, strlen(buffer), HAL_MAX_DELAY);
}

void Telemetry_EncodeCSV(char *buffer, size_t buffer_size, const TelemetryData_t *data, const FaultContext_t *fault_ctx)
{
    uint32_t faults_mask = 0U;
    if (fault_ctx != NULL)
    {
        for (uint32_t i = 0; i <= FAULT_INTERNAL; ++i)
        {
            if (fault_ctx->faults_active[i])
            {
                faults_mask |= (1UL << i);
            }
        }
    }

    /* Формат: V,I,Speed,Temp,State,FaultMask,HOME,REW\r\n */
    snprintf(buffer,
             buffer_size,
             "%.2f,%.2f,%.1f,%.1f,%u,%lu,%u,%u\r\n",
             data->dc_bus_voltage,
             data->phase_current,
             data->speed_rpm,
             data->temperature_c,
             (unsigned)data->motor_state,
             (unsigned long)faults_mask,
             data->home_state ? 1U : 0U,
             data->rew_state ? 1U : 0U);
}

void Telemetry_UpdateContactStates(TelemetryContext_t *ctx, bool home, bool rew)
{
    ctx->latest.home_state = home;
    ctx->latest.rew_state  = rew;
}

static float convert_voltage(uint16_t raw_mv)
{
    /* MCSDK возвращает напряжение в десятых долях вольта. */
    return ((float)raw_mv) * 0.1f;
}

static float convert_current(int16_t raw_ma)
{
    /* MCSDK возвращает ток в миллиамперах. */
    return ((float)raw_ma) / 1000.0f;
}

static float convert_temperature(int16_t raw_decicelsius)
{
    /* MCSDK возвращает температуру в десятых долях градуса. */
    return ((float)raw_decicelsius) / 10.0f;
}
