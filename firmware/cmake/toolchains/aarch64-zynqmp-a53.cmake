# Zynq UltraScale+ MPSoC APU: Arm Cortex-A53, AArch64, bare metal (no OS, EL3/EL1).
# Toolchain: Arm GNU Toolchain aarch64-none-elf (GCC 14, newlib).
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER aarch64-none-elf-gcc)
set(CMAKE_CXX_COMPILER aarch64-none-elf-g++)
set(CMAKE_ASM_COMPILER aarch64-none-elf-gcc)
set(CMAKE_OBJCOPY aarch64-none-elf-objcopy CACHE FILEPATH "")
set(CMAKE_SIZE aarch64-none-elf-size CACHE FILEPATH "")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# -mstrict-align: the image runs with the MMU off, where all memory is Device type
# and an unaligned access faults.
set(EBC_CPU_FLAGS "-mcpu=cortex-a53 -mstrict-align")
set(EBC_COMMON_FLAGS "${EBC_CPU_FLAGS} -ffunction-sections -fdata-sections -fstack-usage")

set(CMAKE_C_FLAGS_INIT "${EBC_COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT
    "${EBC_COMMON_FLAGS} -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit")
set(CMAKE_ASM_FLAGS_INIT "${EBC_CPU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostartfiles -Wl,--gc-sections")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
