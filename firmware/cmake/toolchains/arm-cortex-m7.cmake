# Arm Cortex-M7 microcontrollers (ARMv7E-M, FPv5 double-precision FPU), bare metal.
# Reference board: Arm MPS2 AN500 (emulated by QEMU as -M mps2-an500); the same core
# as STM32H7, i.MX RT1170 or SAM E70.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy CACHE FILEPATH "")
set(CMAKE_SIZE arm-none-eabi-size CACHE FILEPATH "")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(EBC_CPU_FLAGS "-mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb")
set(EBC_COMMON_FLAGS "${EBC_CPU_FLAGS} -ffunction-sections -fdata-sections -fstack-usage")

set(CMAKE_C_FLAGS_INIT "${EBC_COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT
    "${EBC_COMMON_FLAGS} -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit")
set(CMAKE_ASM_FLAGS_INIT "${EBC_CPU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostartfiles -Wl,--gc-sections --specs=nano.specs")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
