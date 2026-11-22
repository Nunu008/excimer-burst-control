# ebc::core - the control law. Header-only, freestanding-friendly: no heap, no
# exceptions, no RTTI, no I/O. Everything a target needs is instantiated in the BSP
# with the scalar type of its FPU.

add_library(ebc_core INTERFACE)
add_library(ebc::core ALIAS ebc_core)

target_include_directories(ebc_core INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(ebc_core INTERFACE cxx_std_23)
