cmake_minimum_required(VERSION 3.14)

# Use `ccache` if it is installed in system's PATH.
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

# Export compilation database in JSON format.
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

project(idle C)

# User Option(s)
set(
  NATIVE_OPT TRUE
  CACHE BOOL
  "Whether or not to optimise for the system being compiled on. This should be true *unless* you are making distributed binaries."
)

add_executable(idle main.c)
target_include_directories(idle PUBLIC .)

## Debug/Release flags.
if (NOT MSVC)
  target_compile_options(idle PRIVATE
    $<$<CONFIG:DEBUG>:-O0 -g3 -ggdb3>
    $<$<CONFIG:RELEASE>:-O3>
  )
  target_link_options(idle PRIVATE
    $<$<CONFIG:DEBUG>:-O0 -g3 -ggdb3>
    $<$<CONFIG:RELEASE>:-O3>
  )

  # Keep internal symbol names in debug mode if possible. See below for clang.
  if (CMAKE_C_COMPILER_ID STREQUAL "GCC")
    target_compile_options(idle PRIVATE $<$<CONFIG:DEBUG>:-rdynamic>)
  endif()

  # -march=native only makes sense when compiling for the compiled system.
  if (NATIVE_OPT)
    target_compile_options(idle PRIVATE
      $<$<CONFIG:RELEASE>:-march=native>
    )
  endif()

else()
  target_compile_options(idle PRIVATE
    $<$<CONFIG:DEBUG>:/Od>
    $<$<CONFIG:RELEASE>:/O2>
  )
endif()

# When compiling with clang-cl, MSVC is also defined, so we put this here instead.
if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
  target_compile_options(idle PRIVATE $<$<CONFIG:DEBUG>:-Wl,-export-dynamic>)
endif()

# Compile options.
if (NOT MSVC)
  target_compile_options(
    idle
    PRIVATE
    -Wall -Wextra
    -Wshadow -Wconversion
    -Werror=return-type -Werror=implicit-function-declaration
    -Werror=implicit-int -Werror=pointer-arith

    -Wno-comment

    -fdiagnostics-color=always
  )
else()
  target_compile_options(
    idle
    PRIVATE
    # Enable "all" warnings.
    /W4

    # Source character set is UTF-8
    /utf-8
  )
endif()

# On Windows, don't suggest the _s nonsense functions.
if (WIN32)
  target_compile_definitions(idle PRIVATE
    _CRT_SECURE_NO_WARNINGS
    _CRT_SECURE_NO_WARNINGS_GLOBALS
    _CRT_NONSTDC_NO_WARNINGS
  )
endif()
