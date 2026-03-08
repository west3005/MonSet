#include "main.h"

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
}

/* ========= UART MSP ========= */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef g = {0};

    if (huart->Instance == USART1) {
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        g.Pin = GPIO_PIN_9 | GPIO_PIN_10;
        g.Mode = GPIO_MODE_AF_PP;
        g.Pull = GPIO_PULLUP;
        g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        g.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOA, &g);
    }
    else if (huart->Instance == USART2) {
        __HAL_RCC_USART2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        g.Pin = GPIO_PIN_2 | GPIO_PIN_3;
        g.Mode = GPIO_MODE_AF_PP;
        g.Pull = GPIO_PULLUP;
        g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        g.Alternate = GPIO_AF7_USART2;
        HAL_GPIO_Init(GPIOA, &g);
    }
    else if (huart->Instance == USART3) {
        __HAL_RCC_USART3_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();

        g.Pin = GPIO_PIN_10 | GPIO_PIN_11;
        g.Mode = GPIO_MODE_AF_PP;
        g.Pull = GPIO_PULLUP;
        g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        g.Alternate = GPIO_AF7_USART3;
        HAL_GPIO_Init(GPIOB, &g);
    }
    else if (huart->Instance == USART6) {
        __HAL_RCC_USART6_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();

        g.Pin = GPIO_PIN_6 | GPIO_PIN_7;
        g.Mode = GPIO_MODE_AF_PP;
        g.Pull = GPIO_PULLUP;
        g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        g.Alternate = GPIO_AF8_USART6;
        HAL_GPIO_Init(GPIOC, &g);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        __HAL_RCC_USART1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);
    }
    else if (huart->Instance == USART2) {
        __HAL_RCC_USART2_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2 | GPIO_PIN_3);
    }
    else if (huart->Instance == USART3) {
        __HAL_RCC_USART3_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10 | GPIO_PIN_11);
    }
    else if (huart->Instance == USART6) {
        __HAL_RCC_USART6_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOC, GPIO_PIN_6 | GPIO_PIN_7);
    }
}

/* ========= I2C MSP ========= */
void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1) {
        GPIO_InitTypeDef g = {0};

        __HAL_RCC_GPIOB_CLK_ENABLE();

        g.Pin = GPIO_PIN_6 | GPIO_PIN_7;
        g.Mode = GPIO_MODE_AF_OD;
        g.Pull = GPIO_PULLUP;
        g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        g.Alternate = GPIO_AF4_I2C1;
        HAL_GPIO_Init(GPIOB, &g);

        __HAL_RCC_I2C1_CLK_ENABLE();
    }
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1) {
        __HAL_RCC_I2C1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6 | GPIO_PIN_7);
    }
}

/* ========= RNG MSP ========= */
void HAL_RNG_MspInit(RNG_HandleTypeDef *hrng)
{
    if (hrng->Instance == RNG) {
        __HAL_RCC_RNG_CLK_ENABLE();
    }
}

void HAL_RNG_MspDeInit(RNG_HandleTypeDef *hrng)
{
    if (hrng->Instance == RNG) {
        __HAL_RCC_RNG_CLK_DISABLE();
    }
}

/* ========= RTC MSP ========= */
void HAL_RTC_MspInit(RTC_HandleTypeDef *hrtc_ptr)
{
    if (hrtc_ptr->Instance == RTC) {
        __HAL_RCC_RTC_ENABLE();
        HAL_NVIC_SetPriority(RTC_WKUP_IRQn, 1, 0);
        HAL_NVIC_EnableIRQ(RTC_WKUP_IRQn);
    }
}

void HAL_RTC_MspDeInit(RTC_HandleTypeDef *hrtc_ptr)
{
    if (hrtc_ptr->Instance == RTC) {
        __HAL_RCC_RTC_DISABLE();
        HAL_NVIC_DisableIRQ(RTC_WKUP_IRQn);
    }
}

/* ========= TIM MSP ========= */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) {
        __HAL_RCC_TIM6_CLK_ENABLE();
        HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 2, 0);
        HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
    }
}

void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) {
        __HAL_RCC_TIM6_CLK_DISABLE();
        HAL_NVIC_DisableIRQ(TIM6_DAC_IRQn);
    }
}
