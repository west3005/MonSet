#include "mbedtls/entropy.h"
#include "mbedtls/platform_time.h"

#include "stm32f4xx_hal.h"
#include "core_cm4.h"

#include <string.h>

/*
 * ВНИМАНИЕ по безопасности:
 * Это "best-effort" источник энтропии БЕЗ ADC и БЕЗ TRNG.
 * Подходит для отладки TLS/сети, но не является полноценной криптографической
 * энтропией уровня production.
 */

static uint32_t xorshift32(uint32_t x)
{
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return x;
}

static uint32_t mix32(uint32_t s, uint32_t v)
{
  s ^= v + 0x9E3779B9u + (s << 6) + (s >> 2);
  return s;
}

static void dwt_enable_cycle_counter(void)
{
  /* Включаем CYCCNT, если ещё не включен */
  if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) == 0u) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  }
  if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0u) {
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  }
}

static uint32_t read_cycle_counter_safe(void)
{
  /* Если DWT недоступен/не включился, вернёт 0 — мы домешаем другие источники */
  return DWT->CYCCNT;
}

static uint32_t gather_noise_word(void)
{
  uint32_t s = 0;

  /* Уникальные ID (не энтропия, но помогает отличаться между платами) */
  s = mix32(s, HAL_GetUIDw0());
  s = mix32(s, HAL_GetUIDw1());
  s = mix32(s, HAL_GetUIDw2());

  /* Время/счётчики (дают джиттер) */
  s = mix32(s, HAL_GetTick());
  s = mix32(s, SysTick->VAL);

  dwt_enable_cycle_counter();
  s = mix32(s, read_cycle_counter_safe());

  /* Джиттер исполнения: небольшой "пустой" цикл + повторные чтения */
  for (volatile uint32_t i = 0; i < 64; i++) {
    s = mix32(s, SysTick->VAL);
    s = mix32(s, HAL_GetTick());
    s = mix32(s, read_cycle_counter_safe());
    s = xorshift32(s);
  }

  return s;
}

/*
 * mbedTLS вызывает это через mbedtls_entropy_func(),
 * т.к. включено MBEDTLS_ENTROPY_HARDWARE_ALT.
 */
int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen)
{
  (void)data;

  if (output == NULL || olen == NULL) return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
  *olen = 0;

  if (len == 0) return 0;

  uint32_t s = 0;

  /* Домешиваем адреса как доп. вариативность между сборками/вызовами */
  s = mix32(s, (uint32_t)(uintptr_t)&s);
  s = mix32(s, (uint32_t)(uintptr_t)output);

  /* Накопим состояние */
  for (int k = 0; k < 8; k++) {
    s = mix32(s, gather_noise_word());
    s = xorshift32(s);
  }

  /* Генерируем len байт */
  for (size_t i = 0; i < len; i++) {
    s = xorshift32(s);
    output[i] = (unsigned char)(s & 0xFFu);

    /* Подмешиваем ещё немного джиттера */
    s = mix32(s, SysTick->VAL);
    s = mix32(s, read_cycle_counter_safe());
  }

  *olen = len;
  return 0;
}

/* У тебя включено MBEDTLS_PLATFORM_MS_TIME_ALT */
mbedtls_ms_time_t mbedtls_ms_time(void)
{
  return (mbedtls_ms_time_t)HAL_GetTick();
}
