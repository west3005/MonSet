################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/socket.c \
../Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/wizchip_conf.c 

C_DEPS += \
./Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/socket.d \
./Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/wizchip_conf.d 

OBJS += \
./Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/socket.o \
./Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/wizchip_conf.o 


# Each subdirectory must supply rules for building sources it contributes
Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/%.o Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/%.su Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/%.cyclo: ../Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/%.c Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG '-DMBEDTLS_CONFIG_FILE="monset_mbedtls_config.h"' -DUSE_HAL_DRIVER -DSTM32F407xx -D_WIZCHIP_=W5500 -c -I../Core/Inc -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/mbedTLS/include/psa" -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../FATFS/Target -I../FATFS/App -I../Middlewares/Third_Party/FatFs/src -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/W5500" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Internet/DNS" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Internet/DHCP" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/mbedTLS" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/mbedTLS/include" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/mbedTLS/include/mbedtls" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/mbedTLS/library" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Middlewares-2f-Third_Party-2f-ioLibrary_Driver-2d-master-2f-Ethernet

clean-Middlewares-2f-Third_Party-2f-ioLibrary_Driver-2d-master-2f-Ethernet:
	-$(RM) ./Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/socket.cyclo ./Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/socket.d ./Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/socket.o ./Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/socket.su ./Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/wizchip_conf.cyclo ./Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/wizchip_conf.d ./Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/wizchip_conf.o ./Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/wizchip_conf.su

.PHONY: clean-Middlewares-2f-Third_Party-2f-ioLibrary_Driver-2d-master-2f-Ethernet

