################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../FATFS/Target/bsp_driver_sd.c \
../FATFS/Target/sd_diskio.c \
../FATFS/Target/user_diskio.c 

C_DEPS += \
./FATFS/Target/bsp_driver_sd.d \
./FATFS/Target/sd_diskio.d \
./FATFS/Target/user_diskio.d 

OBJS += \
./FATFS/Target/bsp_driver_sd.o \
./FATFS/Target/sd_diskio.o \
./FATFS/Target/user_diskio.o 


# Each subdirectory must supply rules for building sources it contributes
FATFS/Target/%.o FATFS/Target/%.su FATFS/Target/%.cyclo: ../FATFS/Target/%.c FATFS/Target/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG '-DMBEDTLS_CONFIG_FILE="monset_mbedtls_config.h"' -DUSE_HAL_DRIVER -DSTM32F407xx -D_WIZCHIP_=W5500 -c -I../Core/Inc -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/mbedTLS/include/psa" -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../FATFS/Target -I../FATFS/App -I../Middlewares/Third_Party/FatFs/src -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/W5500" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Internet/DNS" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Internet/DHCP" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/mbedTLS" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/mbedTLS/include" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/mbedTLS/include/mbedtls" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/mbedTLS/library" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-FATFS-2f-Target

clean-FATFS-2f-Target:
	-$(RM) ./FATFS/Target/bsp_driver_sd.cyclo ./FATFS/Target/bsp_driver_sd.d ./FATFS/Target/bsp_driver_sd.o ./FATFS/Target/bsp_driver_sd.su ./FATFS/Target/sd_diskio.cyclo ./FATFS/Target/sd_diskio.d ./FATFS/Target/sd_diskio.o ./FATFS/Target/sd_diskio.su ./FATFS/Target/user_diskio.cyclo ./FATFS/Target/user_diskio.d ./FATFS/Target/user_diskio.o ./FATFS/Target/user_diskio.su

.PHONY: clean-FATFS-2f-Target

