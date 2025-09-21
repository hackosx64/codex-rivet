#include "power_stage.h"

/*
 * Управление силовой частью платы B-G431B-ESC1.
 * Предполагается, что выводы EN, BRAKE и DIR подключены согласно схеме.
 */

void PowerStage_Init(PowerStageContext_t *ctx,
                     GPIO_TypeDef *enable_port, uint16_t enable_pin,
                     GPIO_TypeDef *brake_port, uint16_t brake_pin,
                     GPIO_TypeDef *direction_port, uint16_t direction_pin)
{
    ctx->enable_port    = enable_port;
    ctx->enable_pin     = enable_pin;
    ctx->brake_port     = brake_port;
    ctx->brake_pin      = brake_pin;
    ctx->direction_port = direction_port;
    ctx->direction_pin  = direction_pin;
    ctx->enabled        = false;

    PowerStage_Enable(ctx, false);
    PowerStage_ApplyBrake(ctx, true);
}

void PowerStage_Enable(PowerStageContext_t *ctx, bool enable)
{
    HAL_GPIO_WritePin(ctx->enable_port, ctx->enable_pin, enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
    ctx->enabled = enable;
}

void PowerStage_ApplyBrake(PowerStageContext_t *ctx, bool brake)
{
    HAL_GPIO_WritePin(ctx->brake_port, ctx->brake_pin, brake ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void PowerStage_SetDirection(PowerStageContext_t *ctx, bool forward)
{
    HAL_GPIO_WritePin(ctx->direction_port, ctx->direction_pin, forward ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void PowerStage_CheckHardwareFaults(PowerStageContext_t *ctx, FaultContext_t *fault_ctx)
{
    /*
     * В данной функции можно опрашивать выводы сигналов ошибки (например, FAULT
     * от драйвера MOSFET) и выставлять соответствующие флаги.
     * Для упрощения приведённой прошивки предполагается, что сигнал ошибки
     * подключён к линии GPIO, настроенной как вход с прерыванием.
     */
    (void)ctx;
    (void)fault_ctx;
    /* Реализация зависит от конкретного подключения. */
}
