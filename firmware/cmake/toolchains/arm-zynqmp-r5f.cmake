# Zynq UltraScale+ MPSoC RPU: Arm Cortex-R5F (ARMv7-R), bare metal.
# Toolchain: Arm GNU Toolchain arm-none-eabi (GCC 14, newlib).
#
# C/C++ is compiled as Thumb-2 for code density in the 64 KiB ATCM; the exception
# vectors and the IRQ entry are ARM-state assembly. VFPv3 single precision matches
# the float instantiation of the control law and the v7-r+fp.sp/hard multilib.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy CACHE FILEPATH "")
set(CMAKE_SIZE arm-none-eabi-size CACHE FILEPATH "")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(EBC_CPU_FLAGS "-mcpu=cortex-r5 -mfpu=vfpv3xd -mfloat-abi=hard -mthumb")
set(EBC_COMMON_FLAGS "${EBC_CPU_FLAGS} -ffunction-sections -fdata-sections -fstack-usage")

set(CMAKE_C_FLAGS_INIT "${EBC_COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT
    "${EBC_COMMON_FLAGS} -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit")
set(CMAKE_ASM_FLAGS_INIT "${EBC_CPU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostartfiles -Wl,--gc-sections --specs=nano.specs")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
