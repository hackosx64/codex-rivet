#include "motor_control.h"
#include "stm32g4xx_hal.h"
#include <math.h>

/* Прототипы внешних функций MCSDK */
extern void MC_StartMotor1(void);
extern void MC_StopMotor1(void);
extern void MC_ProgramSpeedRampMotor1(int16_t targetSpeed, uint16_t durationMs);
extern void MC_SetCurrentReferenceMotor1(int16_t current);
extern int16_t MC_GetMecSpeedAverageMotor1(void);

static float calc_current_limit_ratio(float phase_current);
static void apply_speed_command(MotorControlContext_t *ctx, uint32_t rpm_target, uint32_t duration_ms);

void MotorControl_Init(MotorControlContext_t *ctx, FaultContext_t *fault_ctx, PowerStageContext_t *power_stage)
{
    ctx->state                    = MOTOR_STATE_IDLE;
    ctx->direction                = MOTOR_DIRECTION_FORWARD;
    ctx->target_rpm               = 0U;
    ctx->commanded_rpm            = 0U;
    ctx->force_reverse_request    = false;
    ctx->calibration_in_progress  = false;
    ctx->fault_context            = fault_ctx;
    ctx->power_stage              = power_stage;
    ctx->current_limit_ratio      = 1.0f;
    ctx->last_update_tick         = HAL_GetTick();
}

void MotorControl_RequestStart(MotorControlContext_t *ctx, MotorDirection_t direction)
{
    if (FaultHandler_HasCriticalFault(ctx->fault_context))
    {
        return;
    }

    ctx->direction = direction;
    PowerStage_SetDirection(ctx->power_stage, direction == MOTOR_DIRECTION_FORWARD);
    PowerStage_ApplyBrake(ctx->power_stage, false);
    PowerStage_Enable(ctx->power_stage, true);

    ctx->state = MOTOR_STATE_STARTING;
    ctx->target_rpm = (ctx->target_rpm == 0U) ? APP_MAX_RPM : ctx->target_rpm;
    ctx->last_update_tick = HAL_GetTick();

    MC_StartMotor1();
}

void MotorControl_RequestStop(MotorControlContext_t *ctx)
{
    if (ctx->state == MOTOR_STATE_IDLE)
    {
        return;
    }

    ctx->state = MOTOR_STATE_STOPPING;
    ctx->target_rpm = 0U;
    ctx->last_update_tick = HAL_GetTick();

    /* Плавно снижаем скорость через ramp */
    apply_speed_command(ctx, 0U, (uint32_t)(1000.0f * ((float)ctx->commanded_rpm / (float)APP_RAMP_DOWN_RPM_PER_SEC + 0.5f)));
}

void MotorControl_RequestReverse(MotorControlContext_t *ctx)
{
    ctx->force_reverse_request = true;
    MotorControl_RequestStop(ctx);
}

void MotorControl_RequestSpeed(MotorControlContext_t *ctx, uint32_t rpm)
{
    if (rpm > APP_MAX_RPM)
    {
        rpm = APP_MAX_RPM;
    }
    ctx->target_rpm = rpm;
}

void MotorControl_RequestCalibrate(MotorControlContext_t *ctx)
{
    ctx->calibration_in_progress = true;
    /* Пользовательский вызов процедуры калибровки датчиков MCSDK */
    MC_SetCurrentReferenceMotor1(0);
}

void MotorControl_HandleFault(MotorControlContext_t *ctx, FaultId_t fault)
{
    (void)fault;
    ctx->state = MOTOR_STATE_FAULT;
    ctx->commanded_rpm = 0U;
    ctx->target_rpm = 0U;
    PowerStage_ApplyBrake(ctx->power_stage, true);
    PowerStage_Enable(ctx->power_stage, false);
    MC_StopMotor1();
}

void MotorControl_Run(MotorControlContext_t *ctx, const TelemetryData_t *telemetry)
{
    uint32_t now = HAL_GetTick();
    uint32_t elapsed = now - ctx->last_update_tick;
    if (elapsed == 0U)
    {
        return;
    }
    ctx->last_update_tick = now;

    /* Пересчёт коэффициента ограничения скорости по току */
    ctx->current_limit_ratio = calc_current_limit_ratio(telemetry->phase_current);

    switch (ctx->state)
    {
    case MOTOR_STATE_IDLE:
        break;

    case MOTOR_STATE_STARTING:
        /* Ждём выхода на устойчивое вращение */
        if (fabsf(telemetry->speed_rpm) > 100.0f)
        {
            ctx->state = MOTOR_STATE_RUNNING;
        }
        else
        {
            apply_speed_command(ctx, (uint32_t)(ctx->target_rpm * ctx->current_limit_ratio), 500U);
        }
        break;

    case MOTOR_STATE_RUNNING:
    {
        uint32_t limited_target = (uint32_t)(ctx->target_rpm * ctx->current_limit_ratio);
        uint32_t ramp_step = (uint32_t)(((uint64_t)APP_RAMP_UP_RPM_PER_SEC * elapsed) / 1000ULL);
        if (ramp_step == 0U)
        {
            ramp_step = 1U;
        }

        if (ctx->commanded_rpm < limited_target)
        {
            ctx->commanded_rpm = (ctx->commanded_rpm + ramp_step > limited_target) ? limited_target : ctx->commanded_rpm + ramp_step;
            apply_speed_command(ctx, ctx->commanded_rpm, elapsed);
        }
        else if (ctx->commanded_rpm > limited_target)
        {
            ctx->commanded_rpm = (ctx->commanded_rpm > ramp_step) ? ctx->commanded_rpm - ramp_step : 0U;
            apply_speed_command(ctx, ctx->commanded_rpm, elapsed);
        }

        if (ctx->force_reverse_request && ctx->commanded_rpm == 0U)
        {
            ctx->force_reverse_request = false;
            ctx->direction = (ctx->direction == MOTOR_DIRECTION_FORWARD) ? MOTOR_DIRECTION_REVERSE : MOTOR_DIRECTION_FORWARD;
            PowerStage_SetDirection(ctx->power_stage, ctx->direction == MOTOR_DIRECTION_FORWARD);
            ctx->state = MOTOR_STATE_STARTING;
            MC_StartMotor1();
        }
        break;
    }
    case MOTOR_STATE_STOPPING:
    {
        uint32_t ramp_step = (uint32_t)(((uint64_t)APP_RAMP_DOWN_RPM_PER_SEC * elapsed) / 1000ULL);
        if (ctx->commanded_rpm > ramp_step)
        {
            ctx->commanded_rpm -= ramp_step;
            apply_speed_command(ctx, ctx->commanded_rpm, elapsed);
        }
        else
        {
            ctx->commanded_rpm = 0U;
            apply_speed_command(ctx, 0U, 0U);
            PowerStage_ApplyBrake(ctx->power_stage, true);
            PowerStage_Enable(ctx->power_stage, false);
            ctx->state = ctx->force_reverse_request ? MOTOR_STATE_BRAKING : MOTOR_STATE_IDLE;
        }
        break;
    }
    case MOTOR_STATE_BRAKING:
        if (ctx->force_reverse_request)
        {
            ctx->force_reverse_request = false;
            ctx->direction = (ctx->direction == MOTOR_DIRECTION_FORWARD) ? MOTOR_DIRECTION_REVERSE : MOTOR_DIRECTION_FORWARD;
            PowerStage_SetDirection(ctx->power_stage, ctx->direction == MOTOR_DIRECTION_FORWARD);
            PowerStage_ApplyBrake(ctx->power_stage, false);
            PowerStage_Enable(ctx->power_stage, true);
            ctx->state = MOTOR_STATE_STARTING;
            MC_StartMotor1();
        }
        else
        {
            ctx->state = MOTOR_STATE_IDLE;
        }
        break;

    case MOTOR_STATE_FAULT:
    default:
        /* Ожидаем очистки ошибки */
        break;
    }
}

static float calc_current_limit_ratio(float phase_current)
{
    if (phase_current <= APP_CONTINUOUS_CURRENT_A)
    {
        return 1.0f;
    }
    if (phase_current >= APP_CURRENT_REDUCTION_POINT_A)
    {
        return APP_SPEED_LIMIT_RATIO_MIN;
    }

    float span = APP_CURRENT_REDUCTION_POINT_A - APP_CONTINUOUS_CURRENT_A;
    float over = phase_current - APP_CONTINUOUS_CURRENT_A;
    float ratio = 1.0f - over / span * (1.0f - APP_SPEED_LIMIT_RATIO_MIN);
    if (ratio < APP_SPEED_LIMIT_RATIO_MIN)
    {
        ratio = APP_SPEED_LIMIT_RATIO_MIN;
    }
    if (ratio > 1.0f)
    {
        ratio = 1.0f;
    }
    return ratio;
}

static void apply_speed_command(MotorControlContext_t *ctx, uint32_t rpm_target, uint32_t duration_ms)
{
    ctx->commanded_rpm = rpm_target;
    if (duration_ms == 0U)
    {
        duration_ms = 1U;
    }
    MC_ProgramSpeedRampMotor1((int16_t)((ctx->direction == MOTOR_DIRECTION_FORWARD) ? rpm_target : -((int32_t)rpm_target)), (uint16_t)duration_ms);
}
