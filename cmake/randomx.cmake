# Copyright (c) 2024-present The RNG developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

# RandomX integration for RNG
# RandomX is a CPU-optimized proof-of-work algorithm used by Monero
# and adopted by RNG for ASIC resistance.

enable_language(C)

function(add_randomx subdir)
  message("")
  message("Configuring RandomX subtree...")

  get_filename_component(RANDOMX_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${subdir}" ABSOLUTE)
  execute_process(
    COMMAND ${CMAKE_COMMAND}
      -DRANDOMX_SOURCE_DIR=${RANDOMX_SOURCE_DIR}
      -P ${PROJECT_SOURCE_DIR}/cmake/script/PatchRandomX.cmake
    COMMAND_ERROR_IS_FATAL ANY
  )

  set(BUILD_SHARED_LIBS OFF)
  set(CMAKE_EXPORT_COMPILE_COMMANDS OFF)

  # We don't need RandomX's test/benchmark executables in our build
  # The RandomX CMakeLists.txt creates these automatically, but we'll
  # exclude them from the ALL target later

  include(GetTargetInterface)
  # -fsanitize and related flags apply to both C++ and C,
  # so we can pass them down to RandomX as CFLAGS and LDFLAGS.
  get_target_interface(RANDOMX_APPEND_CFLAGS "" sanitize_interface COMPILE_OPTIONS)
  string(STRIP "${RANDOMX_APPEND_CFLAGS} ${APPEND_CPPFLAGS}" RANDOMX_APPEND_CFLAGS)
  string(STRIP "${RANDOMX_APPEND_CFLAGS} ${APPEND_CFLAGS}" RANDOMX_APPEND_CFLAGS)

  get_target_interface(RANDOMX_APPEND_LDFLAGS "" sanitize_interface LINK_OPTIONS)
  string(STRIP "${RANDOMX_APPEND_LDFLAGS} ${APPEND_LDFLAGS}" RANDOMX_APPEND_LDFLAGS)

  # We want to build RandomX with the most tested RelWithDebInfo configuration.
  foreach(config IN LISTS CMAKE_BUILD_TYPE CMAKE_CONFIGURATION_TYPES)
    if(config STREQUAL "")
      continue()
    endif()
    string(TOUPPER "${config}" config)
    set(CMAKE_C_FLAGS_${config} "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
    set(CMAKE_CXX_FLAGS_${config} "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  endforeach()

  # If the CFLAGS environment variable is defined during building depends
  # and configuring this build system, its content might be duplicated.
  if(DEFINED ENV{CFLAGS})
    deduplicate_flags(CMAKE_C_FLAGS)
  endif()
  if(DEFINED ENV{CXXFLAGS})
    deduplicate_flags(CMAKE_CXX_FLAGS)
  endif()

  add_subdirectory(${subdir})

  # Exclude RandomX library and its test executables from ALL target
  set_target_properties(randomx PROPERTIES
    EXCLUDE_FROM_ALL TRUE
  )

  # Exclude test executables if they exist
  if(TARGET randomx-tests)
    set_target_properties(randomx-tests PROPERTIES
      EXCLUDE_FROM_ALL TRUE
    )
  endif()
  if(TARGET randomx-benchmark)
    set_target_properties(randomx-benchmark PROPERTIES
      EXCLUDE_FROM_ALL TRUE
    )
  endif()
  if(TARGET randomx-codegen)
    set_target_properties(randomx-codegen PROPERTIES
      EXCLUDE_FROM_ALL TRUE
    )
  endif()
endfunction()
