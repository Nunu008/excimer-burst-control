# Warning policy shared by every firmware target.
#
# -Wdouble-promotion matters on Cortex-R5F/M7: a silent float -> double promotion
# turns a one-cycle FPU instruction into a soft-float library call inside the pulse
# interrupt. -Wconversion/-Wsign-conversion catch the index arithmetic bugs that
# bare-metal code cannot afford to find at run time.

add_library(ebc_warnings INTERFACE)
add_library(ebc::warnings ALIAS ebc_warnings)

target_compile_options(ebc_warnings INTERFACE
  -Wall -Wextra -Wpedantic
  -Wconversion -Wsign-conversion -Wdouble-promotion
  -Wshadow -Wcast-align -Wnull-dereference -Wundef
  -Wformat=2 -Wimplicit-fallthrough
  $<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast -Wnon-virtual-dtor -Woverloaded-virtual -Wuseless-cast>
  $<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes -Wmissing-prototypes>
  $<$<BOOL:${EBC_WARNINGS_AS_ERRORS}>:-Werror>)
