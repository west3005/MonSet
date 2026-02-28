/**
 * ================================================================
 * @file    main.cpp
 * @brief   Точка входа. Инициализация HAL, тактирования,
 *          всей периферии и запуск App.
 * ================================================================
 */

#include "main.h"
#include "config.hpp"
#include "debug_uart.hpp"
#include "app.hpp"
#include "spi.h"

extern "C" {
    #include "fatfs.h"
    #include "sdio.h"
}

/* ======== HAL-хэндлы (глобальные) ======== */
/* hsd / hdma_sdio_rx / hdma_sdio_tx — определены в sdio.c */
UART_HandleTypeDef  huart1;
UART_HandleTypeDef  huart2;
UART_HandleTypeDef  huart3;
UART_HandleTypeDef  huart6;
I2C_HandleTypeDef   hi2c1;
RTC_HandleTypeDef   hrtc;
TIM_HandleTypeDef   htim6;

/* ======== Прототипы ======== */
void SystemClock_Config();
static void MX_GPIO_Init();
static void MX_USART1_UART_Init();
static void MX_USART2_UART_Init();
static void MX_USART3_UART_Init();
static void MX_I2C1_Init();
static void MX_RTC_Init();
static void MX_TIM6_Init();

static App app;

/* =============================================================
 *                         main()
 * ============================================================= */
extern "C" int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();

    DBG.init();
    DBG.info("HAL + Clock + GPIO + UART1 : OK");

    MX_I2C1_Init();
    DBG.info("I2C1 (DS3231)              : OK");

    MX_USART2_UART_Init();
    DBG.info("UART2 (SIM800L)            : OK");

    MX_USART3_UART_Init();
    DBG.info("UART3 (RS-485 Modbus)      : OK");

    MX_SPI1_Init();

    MX_SDIO_SD_Init();
    DBG.info("SDIO (SD Card)             : OK");

    MX_FATFS_Init();
    DBG.info("FatFs                      : OK");

    MX_RTC_Init();
    DBG.info("RTC (WakeUp Timer)         : OK");

    MX_TIM6_Init();
    DBG.info("TIM6 (Modbus timeout)      : OK");

    DBG.separator();
    DBG.info("Вся периферия инициализирована. Запуск приложения...");
    DBG.separator();

    app.init();
    app.run();

    return 0;
}

/* =============================================================
 *   SystemClock_Config — HSE 8 MHz → PLL → 168 MHz
 * ============================================================= */
void SystemClock_Config()
{
    RCC_OscInitTypeDef osc = {};
    RCC_ClkInitTypeDef clk = {};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_LSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.LSEState       = RCC_LSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = 4;
    osc.PLL.PLLN       = 168;
    osc.PLL.PLLP       = RCC_PLLP_DIV2;
    osc.PLL.PLLQ       = 7;

    if (HAL_RCC_OscConfig(&osc) != HAL_OK) { Error_Handler(); }

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                       | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) { Error_Handler(); }
}

/* =============================================================
 *   MX_GPIO_Init
 * ============================================================= */
static void MX_GPIO_Init()
{
    GPIO_InitTypeDef g = {};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();

    // Начальные уровни
    HAL_GPIO_WritePin(GPIOC, STATUS_LED_Pin | SIM800_PWR_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, GPIO_PIN_RESET);

    // W5500: CS=1, RST=1 (не в reset)
    HAL_GPIO_WritePin(PIN_W5500_CS_PORT,  PIN_W5500_CS_PIN,  GPIO_PIN_SET);
    HAL_GPIO_WritePin(PIN_W5500_RST_PORT, PIN_W5500_RST_PIN, GPIO_PIN_SET);

    // STATUS_LED + SIM800_PWR
    g.Pin   = STATUS_LED_Pin | SIM800_PWR_Pin;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &g);

    // RS485_DE
    g.Pin   = RS485_DE_Pin;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(RS485_DE_GPIO_Port, &g);

    // MODE_SWITCH (PC0) input pull-up
    g.Pin   = MODE_SWITCH_Pin;
    g.Mode  = GPIO_MODE_INPUT;
    g.Pull  = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(MODE_SWITCH_GPIO_Port, &g);

    // NET_SELECT (PB0) input pull-up
    g.Pin   = PIN_NET_SW_PIN;
    g.Mode  = GPIO_MODE_INPUT;
    g.Pull  = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(PIN_NET_SW_PORT, &g);

    // W5500_CS (PA8) output
    g.Pin   = PIN_W5500_CS_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(PIN_W5500_CS_PORT, &g);

    // W5500_RST (PC3) output
    g.Pin   = PIN_W5500_RST_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(PIN_W5500_RST_PORT, &g);

    // W5500_INT (PC4) input (можно без подтяжки)
    g.Pin   = PIN_W5500_INT_PIN;
    g.Mode  = GPIO_MODE_INPUT;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(PIN_W5500_INT_PORT, &g);
}

/* =============================================================
 *   USART1 — Debug 115200 8N1  (PA9/PA10)
 * ============================================================= */
static void MX_USART1_UART_Init()
{
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) { Error_Handler(); }
}

/* =============================================================
 *   USART2 — SIM800L 115200 8N1  (PA2/PA3)
 * ============================================================= */
static void MX_USART2_UART_Init()
{
    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 9600;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) { Error_Handler(); }
}

/* =============================================================
 *   USART3 — RS-485 Modbus 9600 8N1  (PB10/PB11)
 * ============================================================= */
static void MX_USART3_UART_Init()
{
    huart3.Instance          = USART3;
    huart3.Init.BaudRate     = 9600;
    huart3.Init.WordLength   = UART_WORDLENGTH_8B;
    huart3.Init.StopBits     = UART_STOPBITS_1;
    huart3.Init.Parity       = UART_PARITY_NONE;
    huart3.Init.Mode         = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart3) != HAL_OK) { Error_Handler(); }
}

/* =============================================================
 *   I2C1 — DS3231 100 kHz  (PB6/PB7)
 * ============================================================= */
static void MX_I2C1_Init()
{
    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = 100000;
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2     = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) { Error_Handler(); }
}

/* =============================================================
 *   RTC — LSE 32.768 kHz, WakeUp Timer
 * ============================================================= */
static void MX_RTC_Init()
{
    hrtc.Instance            = RTC;
    hrtc.Init.HourFormat     = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv   = 127;
    hrtc.Init.SynchPrediv    = 255;
    hrtc.Init.OutPut         = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType     = RTC_OUTPUT_TYPE_OPENDRAIN;
    if (HAL_RTC_Init(&hrtc) != HAL_OK) { Error_Handler(); }
    HAL_NVIC_SetPriority(RTC_WKUP_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(RTC_WKUP_IRQn);
}

/* =============================================================
 *   TIM6 — Modbus timeout 1 мс
 *   84 MHz / (83+1) = 1 МГц, период 999 → 1 мс
 * ============================================================= */
static void MX_TIM6_Init()
{
    htim6.Instance               = TIM6;
    htim6.Init.Prescaler         = 83;
    htim6.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim6.Init.Period            = 999;
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim6) != HAL_OK) { Error_Handler(); }
    HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
}

/* =============================================================
 *   Error_Handler — мигание LED + сообщение в UART
 * ============================================================= */
extern "C" void Error_Handler(void)
{
    __disable_irq();
    const char msg[] = "\r\n!!! FATAL ERROR - Error_Handler() !!!\r\n";
    HAL_UART_Transmit(&huart1,
                      reinterpret_cast<const uint8_t*>(msg),
                      sizeof(msg) - 1, 500);
    while (true) {
        HAL_GPIO_TogglePin(STATUS_LED_GPIO_Port, STATUS_LED_Pin);
        for (volatile uint32_t i = 0; i < 300000; i++) { __NOP(); }
    }
}
