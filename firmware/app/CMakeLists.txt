# ebc::app - target-independent application layer: text output, reporting, the
# supervisory lock-step demo and the interrupt-driven software-in-the-loop pulse loop.
add_library(ebc_app INTERFACE)
add_library(ebc::app ALIAS ebc_app)
target_include_directories(ebc_app INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(ebc_app INTERFACE ebc::core)
