#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

#include "app_config.h"
#include "telemetry.h"
#include "fault_handler.h"
#include "power_stage.h"

/* Перечисление направлений вращения */
typedef enum
{
    MOTOR_DIRECTION_FORWARD = 0,
    MOTOR_DIRECTION_REVERSE
} MotorDirection_t;

/* Состояние внутреннего автомата управления */
typedef enum
{
    MOTOR_STATE_IDLE = 0,
    MOTOR_STATE_STARTING,
    MOTOR_STATE_RUNNING,
    MOTOR_STATE_STOPPING,
    MOTOR_STATE_BRAKING,
    MOTOR_STATE_FAULT
} MotorState_t;

/* Контекст с параметрами и состоянием */
typedef struct
{
    MotorState_t        state;
    MotorDirection_t    direction;
    uint32_t            target_rpm;
    uint32_t            commanded_rpm;
    bool                force_reverse_request;
    bool                calibration_in_progress;
    FaultContext_t     *fault_context;
    PowerStageContext_t *power_stage;
    float               current_limit_ratio;
    uint32_t            last_update_tick;
} MotorControlContext_t;

void MotorControl_Init(MotorControlContext_t *ctx, FaultContext_t *fault_ctx, PowerStageContext_t *power_stage);
void MotorControl_RequestStart(MotorControlContext_t *ctx, MotorDirection_t direction);
void MotorControl_RequestStop(MotorControlContext_t *ctx);
void MotorControl_RequestReverse(MotorControlContext_t *ctx);
void MotorControl_RequestSpeed(MotorControlContext_t *ctx, uint32_t rpm);
void MotorControl_RequestCalibrate(MotorControlContext_t *ctx);
void MotorControl_HandleFault(MotorControlContext_t *ctx, FaultId_t fault);
void MotorControl_Run(MotorControlContext_t *ctx, const TelemetryData_t *telemetry);

#endif /* MOTOR_CONTROL_H */
