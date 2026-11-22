ebc_add_image(ebc_cortex_m7
  LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/link.ld
  SOURCES startup.c main.cpp)
