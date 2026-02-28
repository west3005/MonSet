#include "mbedtls/entropy.h"
#include "mbedtls/platform_time.h"
#include "stm32f4xx_hal.h"
/* ВАЖНО: это минимальная заглушка “чтобы собрать”.
   Для реального TLS тут должен быть настоящий источник энтропии. */
int mbedtls_hardware_poll( void *data,
                           unsigned char *output, size_t len, size_t *olen )
{
    (void) data;
    (void) output;
    (void) len;

    *olen = 0;
    return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
}
mbedtls_ms_time_t mbedtls_ms_time(void)
{
    return (mbedtls_ms_time_t) HAL_GetTick();
}
