################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/httpParser.c \
../Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/httpServer.c \
../Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/httpUtil.c 

C_DEPS += \
./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/httpParser.d \
./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/httpServer.d \
./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/httpUtil.d 

OBJS += \
./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/httpParser.o \
./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/httpServer.o \
./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/httpUtil.o 


# Each subdirectory must supply rules for building sources it contributes
Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/%.o Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/%.su Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/%.cyclo: ../Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/%.c Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../FATFS/Target -I../FATFS/App -I../Middlewares/Third_Party/FatFs/src -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet/W5500" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Ethernet" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Internet/DNS" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master/Internet/DHCP" -I"C:/Users/My/STM32CubeIDE/workspace_1.13.2/MonSet/Middlewares/Third_Party/ioLibrary_Driver-master" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Middlewares-2f-Third_Party-2f-ioLibrary_Driver-2d-master-2f-Internet-2f-httpServer

clean-Middlewares-2f-Third_Party-2f-ioLibrary_Driver-2d-master-2f-Internet-2f-httpServer:
	-$(RM) ./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/httpParser.cyclo ./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/httpParser.d ./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/httpParser.o ./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/httpParser.su ./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/httpServer.cyclo ./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/httpServer.d ./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/httpServer.o ./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/httpServer.su ./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/httpUtil.cyclo ./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/httpUtil.d ./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/httpUtil.o ./Middlewares/Third_Party/ioLibrary_Driver-master/Internet/httpServer/httpUtil.su

.PHONY: clean-Middlewares-2f-Third_Party-2f-ioLibrary_Driver-2d-master-2f-Internet-2f-httpServer

