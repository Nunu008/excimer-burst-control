ebc_add_image(ebc_zynqmp_a53
  LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/link.ld
  SOURCES startup.S main.cpp)
