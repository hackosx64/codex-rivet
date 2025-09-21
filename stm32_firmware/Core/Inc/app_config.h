#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>

/*
 * Настройки приложения для платы B-G431B-ESC1.
 * Все значения указаны в соответствии с техническим заданием и могут быть
 * скорректированы при необходимости.
 */

#define APP_SUPPLY_VOLTAGE_MIN_MV        12500U   /* Минимальное допустимое напряжение питания */
#define APP_SUPPLY_VOLTAGE_MAX_MV        24000U   /* Максимальное допустимое напряжение питания */

#define APP_MAX_PHASE_CURRENT_A          35.0f    /* Максимальный фазный ток */
#define APP_CONTINUOUS_CURRENT_A         20.0f    /* Номинальный ток, при котором доступна полная скорость */
#define APP_CURRENT_REDUCTION_POINT_A    30.0f    /* Ток, при котором скорость ограничивается до 70% */

#define APP_MAX_RPM                      50000U   /* Максимальная механическая скорость двигателя */
#define APP_RAMP_UP_RPM_PER_SEC          2000U    /* Скорость плавного разгона */
#define APP_RAMP_DOWN_RPM_PER_SEC        10000U   /* Скорость торможения */

#define APP_PWM_FREQUENCY_HZ             25000U   /* Частота ШИМ */
#define APP_PWM_DEAD_TIME_NS             1000U    /* Мёртвое время */

#define APP_TELEMETRY_PERIOD_MS          100U     /* Период отправки телеметрии по UART */

#define APP_UART_RX_BUFFER_SIZE          128U
#define APP_UART_TX_BUFFER_SIZE          256U

#define APP_LOGGING_SESSION_MAX_SAMPLES  4096U    /* Глубина кольцевого буфера логирования */

/* Настройки температурных порогов */
#define APP_TEMP_WARNING_C               80.0f
#define APP_TEMP_CRITICAL_C              95.0f

/* Значения для алгоритма плавного ограничения скорости по току */
#define APP_SPEED_LIMIT_RATIO_MIN        0.70f

#endif /* APP_CONFIG_H */
