#ifndef MAIN_H
#define MAIN_H

#include "stm32g4xx_hal.h"
#include "app_config.h"
#include "motor_control.h"
#include "telemetry.h"
#include "uart_protocol.h"
#include "power_stage.h"
#include "fault_handler.h"

extern ADC_HandleTypeDef hadc1;
extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;

void Error_Handler(void);

#endif /* MAIN_H */
