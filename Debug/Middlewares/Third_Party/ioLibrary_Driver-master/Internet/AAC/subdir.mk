################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Middlewares/Third_Party/ioLibrary_Driver-master/Internet/AAC/AddressAutoConfig.c 

C_DEPS += \
./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/AAC/AddressAutoConfig.d 

OBJS += \
./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/AAC/AddressAutoConfig.o 


# Each subdirectory must supply rules for building sources it contributes
Middlewares/Third_Party/ioLibrary_Driver-master/Internet/AAC/%.o Middlewares/Third_Party/ioLibrary_Driver-master/Internet/AAC/%.su Middlewares/Third_Party/ioLibrary_Driver-master/Internet/AAC/%.cyclo: ../Middlewares/Third_Party/ioLibrary_Driver-master/Internet/AAC/%.c Middlewares/Third_Party/ioLibrary_Driver-master/Internet/AAC/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG '-DMBEDTLS_CONFIG_FILE="monset_mbedtls_config.h"' -DUSE_HAL_DRIVER -DSTM32F407xx -D_WIZCHIP_=W5500 -c -I../Core/Inc -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/mbedTLS/include/psa" -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../FATFS/Target -I../FATFS/App -I../Middlewares/Third_Party/FatFs/src -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/W5500" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Internet/DNS" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Internet/DHCP" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/mbedTLS" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/mbedTLS/include" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/mbedTLS/include/mbedtls" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/mbedTLS/library" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Middlewares-2f-Third_Party-2f-ioLibrary_Driver-2d-master-2f-Internet-2f-AAC

clean-Middlewares-2f-Third_Party-2f-ioLibrary_Driver-2d-master-2f-Internet-2f-AAC:
	-$(RM) ./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/AAC/AddressAutoConfig.cyclo ./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/AAC/AddressAutoConfig.d ./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/AAC/AddressAutoConfig.o ./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/AAC/AddressAutoConfig.su

.PHONY: clean-Middlewares-2f-Third_Party-2f-ioLibrary_Driver-2d-master-2f-Internet-2f-AAC

