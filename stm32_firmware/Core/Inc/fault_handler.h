#ifndef FAULT_HANDLER_H
#define FAULT_HANDLER_H

#include <stdbool.h>
#include <stdint.h>

/* Перечень возможных ошибок */
typedef enum
{
    FAULT_NONE = 0,
    FAULT_OVERCURRENT,
    FAULT_OVERVOLTAGE,
    FAULT_UNDERVOLTAGE,
    FAULT_OVERTEMPERATURE,
    FAULT_BEMF_ERROR,
    FAULT_COMMUNICATION,
    FAULT_INTERNAL
} FaultId_t;

/* Контекст для хранения активных флагов ошибок */
typedef struct
{
    bool faults_active[FAULT_INTERNAL + 1];
    uint32_t last_fault_timestamp_ms;
} FaultContext_t;

void FaultHandler_Init(FaultContext_t *ctx);
void FaultHandler_SetFault(FaultContext_t *ctx, FaultId_t fault);
void FaultHandler_ClearFault(FaultContext_t *ctx, FaultId_t fault);
bool FaultHandler_IsFaultActive(const FaultContext_t *ctx, FaultId_t fault);
bool FaultHandler_HasCriticalFault(const FaultContext_t *ctx);
uint32_t FaultHandler_GetLastFaultTimestamp(const FaultContext_t *ctx);

#endif /* FAULT_HANDLER_H */
