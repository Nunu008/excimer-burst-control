# Host build of the firmware loop: the same control code, a POSIX/Windows main().
add_executable(ebc_host_sim main.cpp)
target_link_libraries(ebc_host_sim PRIVATE ebc::core ebc::warnings)

# libstdc++ 14/15 ships the Windows console part of std::print in libstdc++exp.
if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_link_libraries(ebc_host_sim PRIVATE stdc++exp)
endif()

if(BUILD_TESTING)
  add_test(NAME host_sim_smoke COMMAND ebc_host_sim --bursts 15)
  # Burst 14 of the standard preset: the hybrid must be inside 5 % max deviation.
  set_tests_properties(host_sim_smoke PROPERTIES
    PASS_REGULAR_EXPRESSION "\n14,hybrid_ilr_pid,[0-4]\\.[0-9]+,")
endif()
