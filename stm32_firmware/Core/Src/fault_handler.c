#include "fault_handler.h"
#include "stm32g4xx_hal.h"
#include <string.h>

/*
 * Реализация обработчика ошибок силовой части и алгоритмов.
 * Все метки времени опираются на глобальный таймер HAL (HAL_GetTick).
 */

void FaultHandler_Init(FaultContext_t *ctx)
{
    memset(ctx->faults_active, 0, sizeof(ctx->faults_active));
    ctx->last_fault_timestamp_ms = 0U;
}

void FaultHandler_SetFault(FaultContext_t *ctx, FaultId_t fault)
{
    if (fault <= FAULT_INTERNAL)
    {
        ctx->faults_active[fault] = true;
        ctx->last_fault_timestamp_ms = HAL_GetTick();
    }
}

void FaultHandler_ClearFault(FaultContext_t *ctx, FaultId_t fault)
{
    if (fault <= FAULT_INTERNAL)
    {
        ctx->faults_active[fault] = false;
    }
}

bool FaultHandler_IsFaultActive(const FaultContext_t *ctx, FaultId_t fault)
{
    if (fault > FAULT_INTERNAL)
    {
        return false;
    }
    return ctx->faults_active[fault];
}

bool FaultHandler_HasCriticalFault(const FaultContext_t *ctx)
{
    return ctx->faults_active[FAULT_OVERCURRENT] ||
           ctx->faults_active[FAULT_OVERVOLTAGE] ||
           ctx->faults_active[FAULT_UNDERVOLTAGE] ||
           ctx->faults_active[FAULT_OVERTEMPERATURE] ||
           ctx->faults_active[FAULT_INTERNAL];
}

uint32_t FaultHandler_GetLastFaultTimestamp(const FaultContext_t *ctx)
{
    return ctx->last_fault_timestamp_ms;
}
