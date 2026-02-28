################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Middlewares/Third_Party/ioLibrary_Driver-master/Internet/TFTP/netutil.c \
../Middlewares/Third_Party/ioLibrary_Driver-master/Internet/TFTP/tftp.c 

C_DEPS += \
./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/TFTP/netutil.d \
./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/TFTP/tftp.d 

OBJS += \
./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/TFTP/netutil.o \
./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/TFTP/tftp.o 


# Each subdirectory must supply rules for building sources it contributes
Middlewares/Third_Party/ioLibrary_Driver-master/Internet/TFTP/%.o Middlewares/Third_Party/ioLibrary_Driver-master/Internet/TFTP/%.su Middlewares/Third_Party/ioLibrary_Driver-master/Internet/TFTP/%.cyclo: ../Middlewares/Third_Party/ioLibrary_Driver-master/Internet/TFTP/%.c Middlewares/Third_Party/ioLibrary_Driver-master/Internet/TFTP/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../FATFS/Target -I../FATFS/App -I../Middlewares/Third_Party/FatFs/src -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/W5500" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Internet/DNS" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Internet/DHCP" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Middlewares-2f-Third_Party-2f-ioLibrary_Driver-2d-master-2f-Internet-2f-TFTP

clean-Middlewares-2f-Third_Party-2f-ioLibrary_Driver-2d-master-2f-Internet-2f-TFTP:
	-$(RM) ./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/TFTP/netutil.cyclo ./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/TFTP/netutil.d ./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/TFTP/netutil.o ./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/TFTP/netutil.su ./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/TFTP/tftp.cyclo ./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/TFTP/tftp.d ./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/TFTP/tftp.o ./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/TFTP/tftp.su

.PHONY: clean-Middlewares-2f-Third_Party-2f-ioLibrary_Driver-2d-master-2f-Internet-2f-TFTP

