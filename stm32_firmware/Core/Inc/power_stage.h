#ifndef POWER_STAGE_H
#define POWER_STAGE_H

#include <stdbool.h>
#include "stm32g4xx_hal.h"
#include "fault_handler.h"

/* Контекст силовой части */
typedef struct
{
    GPIO_TypeDef *enable_port;
    uint16_t      enable_pin;
    GPIO_TypeDef *brake_port;
    uint16_t      brake_pin;
    GPIO_TypeDef *direction_port;
    uint16_t      direction_pin;
    bool          enabled;
} PowerStageContext_t;

void PowerStage_Init(PowerStageContext_t *ctx,
                     GPIO_TypeDef *enable_port, uint16_t enable_pin,
                     GPIO_TypeDef *brake_port, uint16_t brake_pin,
                     GPIO_TypeDef *direction_port, uint16_t direction_pin);
void PowerStage_Enable(PowerStageContext_t *ctx, bool enable);
void PowerStage_ApplyBrake(PowerStageContext_t *ctx, bool brake);
void PowerStage_SetDirection(PowerStageContext_t *ctx, bool forward);
void PowerStage_CheckHardwareFaults(PowerStageContext_t *ctx, FaultContext_t *fault_ctx);

#endif /* POWER_STAGE_H */
