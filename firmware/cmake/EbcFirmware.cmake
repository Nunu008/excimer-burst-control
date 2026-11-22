# Helpers shared by the bare-metal BSPs.

option(EBC_SEMIHOSTING "Report the image verdict as a semihosting exit code (QEMU/debugger)" OFF)

# Raw binary next to the ELF, and the section sizes in the build log.
function(ebc_add_size_report target)
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${target}> $<TARGET_FILE_DIR:${target}>/${target}.bin
    COMMAND ${CMAKE_SIZE} -A $<TARGET_FILE:${target}>
    COMMENT "Size of ${target}"
    VERBATIM)
endfunction()

# Common shape of a bare-metal image: startup, C run time, syscalls, linker script.
function(ebc_add_image target)
  cmake_parse_arguments(ARG "" "LINKER_SCRIPT" "SOURCES" ${ARGN})
  add_executable(${target} ${ARG_SOURCES}
    ${PROJECT_SOURCE_DIR}/bsp/common/crt.c
    ${PROJECT_SOURCE_DIR}/bsp/common/syscalls.c)
  target_include_directories(${target} PRIVATE ${PROJECT_SOURCE_DIR}/bsp/common/include)
  target_link_libraries(${target} PRIVATE ebc::app ebc::warnings)
  target_compile_definitions(${target} PRIVATE $<$<BOOL:${EBC_SEMIHOSTING}>:EBC_SEMIHOSTING>)
  target_link_options(${target} PRIVATE
    -T${ARG_LINKER_SCRIPT}
    -Wl,-Map=$<TARGET_FILE_DIR:${target}>/${target}.map
    -Wl,--print-memory-usage)
  set_target_properties(${target} PROPERTIES SUFFIX ".elf" LINK_DEPENDS ${ARG_LINKER_SCRIPT})
  ebc_add_size_report(${target})
endfunction()
