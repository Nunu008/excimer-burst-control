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
  test_python_parity.cpp
  test_app.cpp)

target_link_libraries(ebc_unit_tests PRIVATE ebc::core ebc::app ebc::warnings GTest::gtest_main)
target_compile_definitions(ebc_unit_tests PRIVATE
  EBC_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")

include(GoogleTest)
gtest_discover_tests(ebc_unit_tests DISCOVERY_MODE PRE_TEST)

# The controller used from plain C23 through ebc.h.
add_executable(ebc_capi_closed_loop capi_closed_loop.c)
target_link_libraries(ebc_capi_closed_loop PRIVATE ebc::capi ebc::warnings m)
set_target_properties(ebc_capi_closed_loop PROPERTIES LINKER_LANGUAGE CXX)
add_test(NAME capi_closed_loop COMMAND ebc_capi_closed_loop)
