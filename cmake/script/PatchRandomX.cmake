# Copyright (c) 2026-present The RNG developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

if(NOT DEFINED RANDOMX_SOURCE_DIR OR RANDOMX_SOURCE_DIR STREQUAL "")
  message(FATAL_ERROR "RANDOMX_SOURCE_DIR must be provided")
endif()

set(RANDOMX_CONFIG_HEADER "${RANDOMX_SOURCE_DIR}/src/configuration.h")
set(RANDOMX_CONFIG_ASM "${RANDOMX_SOURCE_DIR}/src/asm/configuration.asm")

set(RANDOMX_UPSTREAM_SALT "\"RandomX\\x03\"")
set(RNG_CHAIN_SALT "\"RNGCHAIN01\"")
set(RANDOMX_UPSTREAM_ASM_SALT "<\"RandomX\\x03\">")
set(RNG_CHAIN_ASM_SALT "<\"RNGCHAIN01\">")

function(rewrite_file path from to)
  if(NOT EXISTS "${path}")
    return()
  endif()

  file(READ "${path}" original)
  string(REPLACE "${from}" "${to}" updated "${original}")

  if(NOT updated STREQUAL original)
    file(WRITE "${path}" "${updated}")
    message(STATUS "Patched ${path} for RNG mainnet RandomX salt")
  endif()
endfunction()

rewrite_file("${RANDOMX_CONFIG_HEADER}" "${RANDOMX_UPSTREAM_SALT}" "${RNG_CHAIN_SALT}")
rewrite_file("${RANDOMX_CONFIG_ASM}" "${RANDOMX_UPSTREAM_ASM_SALT}" "${RNG_CHAIN_ASM_SALT}")

if(EXISTS "${RANDOMX_CONFIG_HEADER}")
  file(READ "${RANDOMX_CONFIG_HEADER}" patched_header)
  if(NOT patched_header MATCHES "RNGCHAIN01")
    message(FATAL_ERROR "RandomX configuration.h does not contain RNGCHAIN01")
  endif()
endif()

if(EXISTS "${RANDOMX_CONFIG_ASM}")
  file(READ "${RANDOMX_CONFIG_ASM}" patched_asm)
  if(NOT patched_asm MATCHES "RNGCHAIN01")
    message(FATAL_ERROR "RandomX configuration.asm does not contain RNGCHAIN01")
  endif()
endif()
