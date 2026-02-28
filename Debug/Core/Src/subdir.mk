################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../Core/Src/app.cpp \
../Core/Src/data_buffer.cpp \
../Core/Src/debug_uart.cpp \
../Core/Src/ds3231.cpp \
../Core/Src/https_w5500.cpp \
../Core/Src/main.cpp \
../Core/Src/modbus_rtu.cpp \
../Core/Src/power_manager.cpp \
../Core/Src/sd_backup.cpp \
../Core/Src/sd_card_spi.cpp \
../Core/Src/sensor_reader.cpp \
../Core/Src/sim800l.cpp \
../Core/Src/w5500_net.cpp 

C_SRCS += \
../Core/Src/dma.c \
../Core/Src/mbedtls_entropy_alt.c \
../Core/Src/mbedtls_x509_fatfs.c \
../Core/Src/sdio.c \
../Core/Src/spi.c \
../Core/Src/stm32f4xx_hal_msp.c \
../Core/Src/stm32f4xx_hal_sdram.c \
../Core/Src/stm32f4xx_it.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32f4xx.c \
../Core/Src/w5500_port.c \
../Core/Src/wizchip_init.c 

C_DEPS += \
./Core/Src/dma.d \
./Core/Src/mbedtls_entropy_alt.d \
./Core/Src/mbedtls_x509_fatfs.d \
./Core/Src/sdio.d \
./Core/Src/spi.d \
./Core/Src/stm32f4xx_hal_msp.d \
./Core/Src/stm32f4xx_hal_sdram.d \
./Core/Src/stm32f4xx_it.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32f4xx.d \
./Core/Src/w5500_port.d \
./Core/Src/wizchip_init.d 

OBJS += \
./Core/Src/app.o \
./Core/Src/data_buffer.o \
./Core/Src/debug_uart.o \
./Core/Src/dma.o \
./Core/Src/ds3231.o \
./Core/Src/https_w5500.o \
./Core/Src/main.o \
./Core/Src/mbedtls_entropy_alt.o \
./Core/Src/mbedtls_x509_fatfs.o \
./Core/Src/modbus_rtu.o \
./Core/Src/power_manager.o \
./Core/Src/sd_backup.o \
./Core/Src/sd_card_spi.o \
./Core/Src/sdio.o \
./Core/Src/sensor_reader.o \
./Core/Src/sim800l.o \
./Core/Src/spi.o \
./Core/Src/stm32f4xx_hal_msp.o \
./Core/Src/stm32f4xx_hal_sdram.o \
./Core/Src/stm32f4xx_it.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32f4xx.o \
./Core/Src/w5500_net.o \
./Core/Src/w5500_port.o \
./Core/Src/wizchip_init.o 

CPP_DEPS += \
./Core/Src/app.d \
./Core/Src/data_buffer.d \
./Core/Src/debug_uart.d \
./Core/Src/ds3231.d \
./Core/Src/https_w5500.d \
./Core/Src/main.d \
./Core/Src/modbus_rtu.d \
./Core/Src/power_manager.d \
./Core/Src/sd_backup.d \
./Core/Src/sd_card_spi.d \
./Core/Src/sensor_reader.d \
./Core/Src/sim800l.d \
./Core/Src/w5500_net.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.cpp Core/Src/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m4 -std=c++17 -g3 -DDEBUG '-DMBEDTLS_CONFIG_FILE="monset_mbedtls_config.h"' -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../Core/Inc -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/mbedTLS/include/psa" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/mbedTLS/include" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/mbedTLS/include/mbedtls" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/mbedTLS/library" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/W5500" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Internet/DHCP" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Internet/DNS" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Internet/MQTT" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Internet/MQTT/MQTTPacket" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Internet/MQTT/MQTTPacket/src" -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../FATFS/Target -I../FATFS/App -I../Middlewares/Third_Party/FatFs/src -Og -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG '-DMBEDTLS_CONFIG_FILE="monset_mbedtls_config.h"' -DUSE_HAL_DRIVER -DSTM32F407xx -D_WIZCHIP_=W5500 -c -I../Core/Inc -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/mbedTLS/include/psa" -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../FATFS/Target -I../FATFS/App -I../Middlewares/Third_Party/FatFs/src -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/W5500" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Internet/DNS" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Internet/DHCP" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/mbedTLS" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/mbedTLS/include" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/mbedTLS/include/mbedtls" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/mbedTLS/library" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/app.cyclo ./Core/Src/app.d ./Core/Src/app.o ./Core/Src/app.su ./Core/Src/data_buffer.cyclo ./Core/Src/data_buffer.d ./Core/Src/data_buffer.o ./Core/Src/data_buffer.su ./Core/Src/debug_uart.cyclo ./Core/Src/debug_uart.d ./Core/Src/debug_uart.o ./Core/Src/debug_uart.su ./Core/Src/dma.cyclo ./Core/Src/dma.d ./Core/Src/dma.o ./Core/Src/dma.su ./Core/Src/ds3231.cyclo ./Core/Src/ds3231.d ./Core/Src/ds3231.o ./Core/Src/ds3231.su ./Core/Src/https_w5500.cyclo ./Core/Src/https_w5500.d ./Core/Src/https_w5500.o ./Core/Src/https_w5500.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/mbedtls_entropy_alt.cyclo ./Core/Src/mbedtls_entropy_alt.d ./Core/Src/mbedtls_entropy_alt.o ./Core/Src/mbedtls_entropy_alt.su ./Core/Src/mbedtls_x509_fatfs.cyclo ./Core/Src/mbedtls_x509_fatfs.d ./Core/Src/mbedtls_x509_fatfs.o ./Core/Src/mbedtls_x509_fatfs.su ./Core/Src/modbus_rtu.cyclo ./Core/Src/modbus_rtu.d ./Core/Src/modbus_rtu.o ./Core/Src/modbus_rtu.su ./Core/Src/power_manager.cyclo ./Core/Src/power_manager.d ./Core/Src/power_manager.o ./Core/Src/power_manager.su ./Core/Src/sd_backup.cyclo ./Core/Src/sd_backup.d ./Core/Src/sd_backup.o ./Core/Src/sd_backup.su ./Core/Src/sd_card_spi.cyclo ./Core/Src/sd_card_spi.d ./Core/Src/sd_card_spi.o ./Core/Src/sd_card_spi.su ./Core/Src/sdio.cyclo ./Core/Src/sdio.d ./Core/Src/sdio.o ./Core/Src/sdio.su ./Core/Src/sensor_reader.cyclo ./Core/Src/sensor_reader.d ./Core/Src/sensor_reader.o ./Core/Src/sensor_reader.su ./Core/Src/sim800l.cyclo ./Core/Src/sim800l.d ./Core/Src/sim800l.o ./Core/Src/sim800l.su ./Core/Src/spi.cyclo ./Core/Src/spi.d ./Core/Src/spi.o ./Core/Src/spi.su ./Core/Src/stm32f4xx_hal_msp.cyclo ./Core/Src/stm32f4xx_hal_msp.d ./Core/Src/stm32f4xx_hal_msp.o ./Core/Src/stm32f4xx_hal_msp.su ./Core/Src/stm32f4xx_hal_sdram.cyclo ./Core/Src/stm32f4xx_hal_sdram.d ./Core/Src/stm32f4xx_hal_sdram.o ./Core/Src/stm32f4xx_hal_sdram.su ./Core/Src/stm32f4xx_it.cyclo ./Core/Src/stm32f4xx_it.d ./Core/Src/stm32f4xx_it.o ./Core/Src/stm32f4xx_it.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32f4xx.cyclo ./Core/Src/system_stm32f4xx.d ./Core/Src/system_stm32f4xx.o ./Core/Src/system_stm32f4xx.su ./Core/Src/w5500_net.cyclo ./Core/Src/w5500_net.d ./Core/Src/w5500_net.o ./Core/Src/w5500_net.su ./Core/Src/w5500_port.cyclo ./Core/Src/w5500_port.d ./Core/Src/w5500_port.o ./Core/Src/w5500_port.su ./Core/Src/wizchip_init.cyclo ./Core/Src/wizchip_init.d ./Core/Src/wizchip_init.o ./Core/Src/wizchip_init.su

.PHONY: clean-Core-2f-Src

