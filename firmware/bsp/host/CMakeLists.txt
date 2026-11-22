# Host build of the firmware application: the same code as the Cortex-A53 image,
# with a stdout sink instead of the PS UART.
add_executable(ebc_host_sim main.cpp)
target_link_libraries(ebc_host_sim PRIVATE ebc::app ebc::warnings)

if(BUILD_TESTING)
  # The verdict (hybrid < 5 % max deviation and < 0.5 x PID RMS at the last burst)
  # is the exit code, exactly as the target images report it through semihosting.
  add_test(NAME host_sim_standard COMMAND ebc_host_sim --bursts 15)
  add_test(NAME host_sim_standard_float COMMAND ebc_host_sim --bursts 15 --float)
  add_test(NAME host_sim_latency COMMAND ebc_host_sim --bursts 15 --preset latency)
endif()
