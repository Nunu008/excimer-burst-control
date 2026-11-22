include(FetchContent)

FetchContent_Declare(googletest
  URL https://github.com/google/googletest/releases/download/v1.15.2/googletest-1.15.2.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  EXCLUDE_FROM_ALL)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(ebc_unit_tests
  test_constexpr.cpp
  test_q_filter.cpp
  test_pid.cpp
  test_ilc.cpp
  test_delay_line.cpp
  test_metrics.cpp
  test_noise.cpp
  test_lockstep_engine.cpp
  test_realtime_controller.cpp
  test_python_parity.cpp)

target_link_libraries(ebc_unit_tests PRIVATE ebc::core ebc::warnings GTest::gtest_main)
target_compile_definitions(ebc_unit_tests PRIVATE
  EBC_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")

include(GoogleTest)
gtest_discover_tests(ebc_unit_tests DISCOVERY_MODE PRE_TEST)
