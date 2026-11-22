# ebc::capi - C23 interface for C code bases. Static library, no heap.
add_library(ebc_capi STATIC src/ebc_capi.cpp)
add_library(ebc::capi ALIAS ebc_capi)
target_include_directories(ebc_capi PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(ebc_capi PRIVATE ebc::core ebc::warnings)
