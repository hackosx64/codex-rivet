#include "main.h"
#include <string.h>

ADC_HandleTypeDef hadc1;
UART_HandleTypeDef huart1;
TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;

/* Константы подключения (уточните при разводке) */
#define ESC_ENABLE_GPIO_Port   GPIOB
#define ESC_ENABLE_Pin         GPIO_PIN_0
#define ESC_BRAKE_GPIO_Port    GPIOB
#define ESC_BRAKE_Pin          GPIO_PIN_1
#define ESC_DIR_GPIO_Port      GPIOB
#define ESC_DIR_Pin            GPIO_PIN_2

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM2_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_ADC1_Init();
    MX_USART1_UART_Init();
    MX_TIM1_Init();
    MX_TIM2_Init();

    FaultContext_t fault_ctx;
    PowerStageContext_t power_stage;
    MotorControlContext_t motor_ctx;
    TelemetryContext_t telemetry_ctx;
    UARTProtocolContext_t uart_ctx;

    FaultHandler_Init(&fault_ctx);
    PowerStage_Init(&power_stage,
                    ESC_ENABLE_GPIO_Port, ESC_ENABLE_Pin,
                    ESC_BRAKE_GPIO_Port, ESC_BRAKE_Pin,
                    ESC_DIR_GPIO_Port, ESC_DIR_Pin);

    MotorControl_Init(&motor_ctx, &fault_ctx, &power_stage);
    Telemetry_Init(&telemetry_ctx, &hadc1, &huart1, &htim1, &fault_ctx);
    UARTProtocol_Init(&uart_ctx, &huart1, &motor_ctx, &telemetry_ctx, &fault_ctx);

    uint32_t last_monitor_tick = HAL_GetTick();

    while (1)
    {
        Telemetry_Update(&telemetry_ctx, &fault_ctx, motor_ctx.state);
        const TelemetryData_t *telemetry = Telemetry_GetLatest(&telemetry_ctx);

        /* Проверка напряжения */
        if (telemetry->dc_bus_voltage < (APP_SUPPLY_VOLTAGE_MIN_MV / 1000.0f))
        {
            FaultHandler_SetFault(&fault_ctx, FAULT_UNDERVOLTAGE);
        }
        else if (telemetry->dc_bus_voltage > (APP_SUPPLY_VOLTAGE_MAX_MV / 1000.0f))
        {
            FaultHandler_SetFault(&fault_ctx, FAULT_OVERVOLTAGE);
        }
        else
        {
            FaultHandler_ClearFault(&fault_ctx, FAULT_UNDERVOLTAGE);
            FaultHandler_ClearFault(&fault_ctx, FAULT_OVERVOLTAGE);
        }

        /* Проверка тока */
        if (telemetry->phase_current > APP_MAX_PHASE_CURRENT_A)
        {
            FaultHandler_SetFault(&fault_ctx, FAULT_OVERCURRENT);
        }
        else if (telemetry->phase_current < APP_MAX_PHASE_CURRENT_A * 0.9f)
        {
            FaultHandler_ClearFault(&fault_ctx, FAULT_OVERCURRENT);
        }

        /* Проверка температуры */
        if (telemetry->temperature_c > APP_TEMP_CRITICAL_C)
        {
            FaultHandler_SetFault(&fault_ctx, FAULT_OVERTEMPERATURE);
        }
        else if (telemetry->temperature_c < APP_TEMP_WARNING_C)
        {
            FaultHandler_ClearFault(&fault_ctx, FAULT_OVERTEMPERATURE);
        }

        PowerStage_CheckHardwareFaults(&power_stage, &fault_ctx);

        if (FaultHandler_HasCriticalFault(&fault_ctx))
        {
            MotorControl_HandleFault(&motor_ctx, FAULT_INTERNAL);
        }

        MotorControl_Run(&motor_ctx, telemetry);
        UARTProtocol_Process(&uart_ctx);
        Telemetry_TransmitIfDue(&telemetry_ctx);

        if ((HAL_GetTick() - last_monitor_tick) > 1000U)
        {
            last_monitor_tick = HAL_GetTick();
            UARTProtocol_SendStatus(&uart_ctx);
        }

        HAL_Delay(1);
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV2;
    RCC_OscInitStruct.PLL.PLLN = 85;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    {
        Error_Handler();
    }

    SystemCoreClockUpdate();
}

static void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc1.Init.LowPowerAutoWait = DISABLE;
    hadc1.Init.ContinuousConvMode = ENABLE;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    /* Настроить канал измерения напряжения DC bus */
    sConfig.Channel = ADC_CHANNEL_1;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_12CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_TIM1_Init(void)
{
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    htim1.Instance = TIM1;
    htim1.Init.Prescaler = 0;
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.Period = (uint32_t)((SystemCoreClock / APP_PWM_FREQUENCY_HZ) - 1U);
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
    {
        Error_Handler();
    }

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
    {
        Error_Handler();
    }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_TIM2_Init(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = (SystemCoreClock / 1000000U) - 1U;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 0xFFFFFFFFU;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
    {
        Error_Handler();
    }
    HAL_TIM_Base_Start(&htim2);
}

static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(ESC_ENABLE_GPIO_Port, ESC_ENABLE_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(ESC_BRAKE_GPIO_Port, ESC_BRAKE_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(ESC_DIR_GPIO_Port, ESC_DIR_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = ESC_ENABLE_Pin | ESC_BRAKE_Pin | ESC_DIR_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(ESC_ENABLE_GPIO_Port, &GPIO_InitStruct);
}
