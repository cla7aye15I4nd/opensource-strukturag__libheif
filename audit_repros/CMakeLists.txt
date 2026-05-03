# Audit reproducer harnesses.
#
# Wired into the parent build via:
#   if(ENABLE_AUDIT_REPROS)
#     add_subdirectory(audit_repros)
#   endif()
#
# Each harness links the static libheif target so it can call the public C
# API (heif.h) and, where needed, the internal C++ headers (StreamReader,
# BitstreamRange, DataExtent, Box_idat).

cmake_minimum_required(VERSION 3.16.3)

set(_LIBHEIF_INTERNAL_INCLUDES
  "${CMAKE_SOURCE_DIR}/audit_repros"     # for "common/heif_decode_harness.h"
  "${CMAKE_SOURCE_DIR}/libheif"          # for "bitstream.h", "box.h", "file.h"
  "${CMAKE_SOURCE_DIR}/libheif/api"      # for "libheif/heif.h"
  "${CMAKE_SOURCE_DIR}"                  # for "libheif/heif.h" (alt path)
  "${CMAKE_BINARY_DIR}"                  # for generated config headers
)

# heif (the static target) is defined in libheif/CMakeLists.txt.
# Some bugs are reachable only via internal classes — those harnesses include
# bitstream.h / box.h / decoder.h directly and link the same static target.

function(add_audit_repro name)
  cmake_parse_arguments(R "" "" "SOURCES;EXTRA_DEFS" ${ARGN})
  add_executable(${name} ${R_SOURCES})
  target_include_directories(${name} PRIVATE ${_LIBHEIF_INTERNAL_INCLUDES})
  target_link_libraries(${name} PRIVATE heif)
  # ABI parity with the libheif target — these defines change the layout of
  # HeifFile / Box_iloc / etc. The harness must see the same ones libheif
  # was compiled with, otherwise sizeof(class) differs and stack-allocated
  # objects trigger spurious stack-buffer-overflows.
  if (ENABLE_MULTITHREADING_SUPPORT)
    target_compile_definitions(${name} PRIVATE ENABLE_MULTITHREADING_SUPPORT=1)
    if (ENABLE_PARALLEL_TILE_DECODING)
      target_compile_definitions(${name} PRIVATE ENABLE_PARALLEL_TILE_DECODING=1)
    endif()
  endif()
  if (ENABLE_PLUGIN_LOADING)
    target_compile_definitions(${name} PRIVATE ENABLE_PLUGIN_LOADING=1)
  endif()
  if (ENABLE_EXPERIMENTAL_MINI_FORMAT)
    target_compile_definitions(${name} PRIVATE ENABLE_EXPERIMENTAL_MINI_FORMAT=1)
  endif()
  target_compile_definitions(${name} PRIVATE ${R_EXTRA_DEFS})
  set_target_properties(${name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/audit_repros")
endfunction()

# Public-API harnesses (consume a HEIF file).
add_audit_repro(bug02 SOURCES bug02_bitstreamrange_end_lt_start/harness.cc)
add_audit_repro(bug04 SOURCES bug04_iloc_field_size_misalignment/harness.cc)
add_audit_repro(bug05 SOURCES bug05_iloc_base_offset_size/harness.cc)
add_audit_repro(bug06 SOURCES bug06_iloc_offset_size/harness.cc)
add_audit_repro(bug07 SOURCES bug07_iloc_length_size/harness.cc)
add_audit_repro(bug09 SOURCES bug09_idat_read_data_ignored/harness.cc)
add_audit_repro(bug10 SOURCES bug10_iloc_size_underflow/harness.cc)
add_audit_repro(bug17 SOURCES bug17_file_layout_meta_box_overflow/harness.cc)
add_audit_repro(bug18 SOURCES bug18_file_layout_moov_box_overflow/harness.cc)

# Internal-class harnesses (no HEIF file; drive a class directly).
add_audit_repro(bug01 SOURCES bug01_streamreader_memory_read_overflow/harness.cc)
add_audit_repro(bug03 SOURCES bug03_wait_for_available_bytes_signed_overflow/harness.cc)
add_audit_repro(bug08 SOURCES bug08_iloc_extent_sum_overflow_safe/harness.cc)
add_audit_repro(bug11 SOURCES bug11_idat_data_start_pos_overflow/harness.cc)
add_audit_repro(bug12 SOURCES bug12_idat_start_plus_length_safe/harness.cc)
add_audit_repro(bug13 SOURCES bug13_idat_signed_unsigned_mix/harness.cc)
add_audit_repro(bug14 SOURCES bug14_idat_seek_assert/harness.cc)
add_audit_repro(bug15 SOURCES bug15_dataextent_oob_iter/harness.cc)
add_audit_repro(bug16 SOURCES bug16_file_layout_header_end_overflow/harness.cc)
