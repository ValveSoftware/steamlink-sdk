##  Copyright (c) 2015 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
cmake_minimum_required(VERSION 3.2)
project(LIBWEBM CXX)

include("${CMAKE_CURRENT_SOURCE_DIR}/build/msvc_runtime.cmake")

set(LIBWEBM_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# Build/test configuration flags. Defined here for visibility.
option(ENABLE_WEBMTS "Enables WebM PES/TS support." ON)
option(ENABLE_WEBMINFO "Enables building webm_info." ON)
option(ENABLE_TESTS "Enables tests." OFF)
option(ENABLE_IWYU "Enables include-what-you-use support." OFF)
option(ENABLE_WERROR "Enable warnings as errors." OFF)
set(GTEST_SRC_DIR "${LIBWEBM_SRC_DIR}/../googletest" CACHE PATH
    "Path to Googletest git repository.")

# This directory is where libwebm will build googletest dependencies.
set(GTEST_BUILD_DIR "${CMAKE_BINARY_DIR}/googletest_build")

include("${CMAKE_CURRENT_SOURCE_DIR}/build/cxx_flags.cmake")
if (ENABLE_TESTS OR ENABLE_WEBMTS OR ENABLE_WEBMINFO)
  include("${CMAKE_CURRENT_SOURCE_DIR}/build/cxx11_tests.cmake")
endif ()

set(CMAKE_CXX_FLAGS "-D__STDC_CONSTANT_MACROS ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "-D__STDC_FORMAT_MACROS ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "-D__STDC_LIMIT_MACROS ${CMAKE_CXX_FLAGS}")

# mkvmuxer section.
# TODO(tomfinegan): move to mkvmuxer/CMakeLists.txt
add_library(mkvmuxer STATIC
            "${LIBWEBM_SRC_DIR}/mkvmuxer/mkvmuxer.cc"
            "${LIBWEBM_SRC_DIR}/mkvmuxer/mkvmuxer.h"
            "${LIBWEBM_SRC_DIR}/mkvmuxer/mkvmuxertypes.h"
            "${LIBWEBM_SRC_DIR}/mkvmuxer/mkvmuxerutil.cc"
            "${LIBWEBM_SRC_DIR}/mkvmuxer/mkvmuxerutil.h"
            "${LIBWEBM_SRC_DIR}/mkvmuxer/mkvwriter.cc"
            "${LIBWEBM_SRC_DIR}/mkvmuxer/mkvwriter.h"
            "${LIBWEBM_SRC_DIR}/common/webmids.h")

# mkvparser section.
# TODO(tomfinegan): move to mkvparser/CMakeLists.txt.
add_library(mkvparser STATIC
            "${LIBWEBM_SRC_DIR}/mkvparser/mkvparser.cc"
            "${LIBWEBM_SRC_DIR}/mkvparser/mkvparser.h"
            "${LIBWEBM_SRC_DIR}/mkvparser/mkvreader.cc"
            "${LIBWEBM_SRC_DIR}/mkvparser/mkvreader.h"
            "${LIBWEBM_SRC_DIR}/common/webmids.h")

# Libwebm section.
add_library(webm STATIC
            "${LIBWEBM_SRC_DIR}/common/file_util.cc"
            "${LIBWEBM_SRC_DIR}/common/file_util.h"
            "${LIBWEBM_SRC_DIR}/common/hdr_util.cc"
            "${LIBWEBM_SRC_DIR}/common/hdr_util.h"
            "${LIBWEBM_SRC_DIR}/common/webmids.h")
target_link_libraries(webm LINK_PUBLIC mkvmuxer mkvparser)

if (WIN32)
  # Use libwebm and libwebm.lib for project and library name on Windows (instead
  # webm and webm.lib).
  set_target_properties(webm PROPERTIES PROJECT_LABEL libwebm)
  set_target_properties(webm PROPERTIES PREFIX lib)
endif ()

include_directories("${LIBWEBM_SRC_DIR}")

# Sample (mkvparser) section.
add_executable(sample
               "${LIBWEBM_SRC_DIR}/sample.cc")
target_link_libraries(sample LINK_PUBLIC webm)

# Sample muxer section.
add_executable(sample_muxer
               "${LIBWEBM_SRC_DIR}/sample_muxer.cc"
               "${LIBWEBM_SRC_DIR}/sample_muxer_metadata.cc"
               "${LIBWEBM_SRC_DIR}/sample_muxer_metadata.h"
               "${LIBWEBM_SRC_DIR}/webvtt/vttreader.cc"
               "${LIBWEBM_SRC_DIR}/webvtt/vttreader.h"
               "${LIBWEBM_SRC_DIR}/webvtt/webvttparser.cc"
               "${LIBWEBM_SRC_DIR}/webvtt/webvttparser.h")
target_link_libraries(sample_muxer LINK_PUBLIC webm)

# Dumpvtt section.
add_executable(dumpvtt
               "${LIBWEBM_SRC_DIR}/dumpvtt.cc"
               "${LIBWEBM_SRC_DIR}/webvtt/vttreader.cc"
               "${LIBWEBM_SRC_DIR}/webvtt/vttreader.h"
               "${LIBWEBM_SRC_DIR}/webvtt/webvttparser.cc"
               "${LIBWEBM_SRC_DIR}/webvtt/webvttparser.h")
target_link_libraries(dumpvtt LINK_PUBLIC webm)

# Vttdemux section.
add_executable(vttdemux
               "${LIBWEBM_SRC_DIR}/vttdemux.cc"
               "${LIBWEBM_SRC_DIR}/webvtt/webvttparser.cc"
               "${LIBWEBM_SRC_DIR}/webvtt/webvttparser.h")
target_link_libraries(vttdemux LINK_PUBLIC webm)

if (ENABLE_WEBMINFO)
# Webm_info section.
add_executable(webm_info
               "${LIBWEBM_SRC_DIR}/common/indent.cc"
               "${LIBWEBM_SRC_DIR}/common/indent.h"
               "${LIBWEBM_SRC_DIR}/common/vp9_header_parser.cc"
               "${LIBWEBM_SRC_DIR}/common/vp9_header_parser.h"
               "${LIBWEBM_SRC_DIR}/common/vp9_level_stats.cc"
               "${LIBWEBM_SRC_DIR}/common/vp9_level_stats.h"
               "${LIBWEBM_SRC_DIR}/common/webm_constants.h"
               "${LIBWEBM_SRC_DIR}/common/webm_endian.cc"
               "${LIBWEBM_SRC_DIR}/common/webm_endian.h"
               "${LIBWEBM_SRC_DIR}/webm_info.cc")
target_link_libraries(webm_info LINK_PUBLIC webm)
endif ()

if (ENABLE_WEBMTS)
  # webmts (PES/TS support) library section.
  add_library(webmts
              "${LIBWEBM_SRC_DIR}/common/libwebm_util.cc"
              "${LIBWEBM_SRC_DIR}/common/libwebm_util.h"
              "${LIBWEBM_SRC_DIR}/m2ts/vpxpes2ts.cc"
              "${LIBWEBM_SRC_DIR}/m2ts/vpxpes2ts.h"
              "${LIBWEBM_SRC_DIR}/m2ts/vpxpes_parser.cc"
              "${LIBWEBM_SRC_DIR}/m2ts/vpxpes_parser.h"
              "${LIBWEBM_SRC_DIR}/m2ts/webm2pes.cc"
              "${LIBWEBM_SRC_DIR}/m2ts/webm2pes.h")

  # webm2pes section.
  add_executable(webm2pes
                "${LIBWEBM_SRC_DIR}/m2ts/webm2pes_main.cc")
  target_link_libraries(webm2pes LINK_PUBLIC webm webmts)

  # webm2ts section.
  add_executable(webm2ts "${LIBWEBM_SRC_DIR}/m2ts/vpxpes2ts_main.cc")
  target_link_libraries(webm2ts LINK_PUBLIC webm webmts)
endif ()

if (ENABLE_TESTS)
  add_subdirectory("${GTEST_SRC_DIR}" "${GTEST_BUILD_DIR}")
  include_directories("${GTEST_SRC_DIR}/googletest/include")

  add_executable(muxer_tests
                 "${LIBWEBM_SRC_DIR}/testing/muxer_tests.cc"
                 "${LIBWEBM_SRC_DIR}/testing/test_util.cc"
                 "${LIBWEBM_SRC_DIR}/testing/test_util.h")
  target_link_libraries(muxer_tests LINK_PUBLIC gtest webm)

  add_executable(parser_tests
                 "${LIBWEBM_SRC_DIR}/testing/parser_tests.cc"
                 "${LIBWEBM_SRC_DIR}/testing/test_util.cc"
                 "${LIBWEBM_SRC_DIR}/testing/test_util.h")
  target_link_libraries(parser_tests LINK_PUBLIC gtest webm)

  add_executable(vp9_header_parser_tests
                 "${LIBWEBM_SRC_DIR}/common/vp9_header_parser_tests.cc"
                 "${LIBWEBM_SRC_DIR}/common/vp9_header_parser.cc"
                 "${LIBWEBM_SRC_DIR}/common/vp9_header_parser.h"
                 "${LIBWEBM_SRC_DIR}/testing/test_util.cc"
                 "${LIBWEBM_SRC_DIR}/testing/test_util.h")
  target_link_libraries(vp9_header_parser_tests LINK_PUBLIC gtest webm)

  add_executable(vp9_level_stats_tests
                 "${LIBWEBM_SRC_DIR}/common/vp9_header_parser.cc"
                 "${LIBWEBM_SRC_DIR}/common/vp9_header_parser.h"
                 "${LIBWEBM_SRC_DIR}/common/vp9_level_stats_tests.cc"
                 "${LIBWEBM_SRC_DIR}/common/vp9_level_stats.cc"
                 "${LIBWEBM_SRC_DIR}/common/vp9_level_stats.h"
                 "${LIBWEBM_SRC_DIR}/testing/test_util.cc"
                 "${LIBWEBM_SRC_DIR}/testing/test_util.h")
  target_link_libraries(vp9_level_stats_tests LINK_PUBLIC gtest webm)

  if (ENABLE_WEBMTS)
    add_executable(webm2pes_tests
                  "${LIBWEBM_SRC_DIR}/testing/test_util.cc"
                  "${LIBWEBM_SRC_DIR}/testing/test_util.h"
                  "${LIBWEBM_SRC_DIR}/m2ts/tests/webm2pes_tests.cc")
    target_link_libraries(webm2pes_tests LINK_PUBLIC gtest webm webmts)
  endif ()
endif ()

# Include-what-you-use section.
if (ENABLE_IWYU)
  # Make sure all the tools necessary for IWYU are present.
  find_program(iwyu_path NAMES include-what-you-use)
  find_program(iwyu_tool_path NAMES iwyu_tool.py)

  # Some odd behavior on cmake's part: PYTHON_EXECUTABLE and PYTHON_VERSION_*
  # are set by cmake when it does its internal python check, but
  # PYTHONINTERP_FOUND is empty without explicitly looking for it.
  find_package(PythonInterp)

  if (iwyu_path AND iwyu_tool_path AND PYTHONINTERP_FOUND)
    # Enable compilation command export (needed for iwyu_tool.py)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    # Add a custom target to run iwyu across all targets.
    add_custom_target(iwyu
                      ALL
                      COMMAND "${PYTHON_EXECUTABLE}" "${iwyu_tool_path}" -p
                          "${CMAKE_BINARY_DIR}"
                      COMMENT "Running include-what-you-use..."
                      VERBATIM)
  else ()
    message(STATUS "Ignoring ENABLE_IWYU because reasons:")
    message(STATUS "  iwyu_path=" ${iwyu_path})
    message(STATUS "  iwyu_tool_path=" ${iwyu_tool_path})
    message(STATUS "  PYTHONINTERP_FOUND=" ${PYTHONINTERP_FOUND})
    message(STATUS "  See README.libwebm for more information.")
  endif ()
endif ()
