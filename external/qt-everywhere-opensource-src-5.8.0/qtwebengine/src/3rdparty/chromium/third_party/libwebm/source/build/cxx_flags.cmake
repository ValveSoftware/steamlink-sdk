##  Copyright (c) 2016 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
cmake_minimum_required(VERSION 3.2)

include(CheckCXXCompilerFlag)

function (add_cxx_flag_if_supported cxx_flag)
  unset(FLAG_SUPPORTED CACHE)
  message("Checking compiler flag support for: " ${cxx_flag})
  CHECK_CXX_COMPILER_FLAG("${cxx_flag}" FLAG_SUPPORTED)
  if (FLAG_SUPPORTED)
    set(CMAKE_CXX_FLAGS "${cxx_flag} ${CMAKE_CXX_FLAGS}" CACHE STRING "" FORCE)
  endif ()
endfunction ()

# Set warning levels.
if (MSVC)
  add_cxx_flag_if_supported("/W4")
  # Disable MSVC warnings that suggest making code non-portable.
  add_cxx_flag_if_supported("/wd4996")
  if (ENABLE_WERROR)
    add_cxx_flag_if_supported("/WX")
  endif ()
else ()
  add_cxx_flag_if_supported("-Wall")
  add_cxx_flag_if_supported("-Wextra")
  add_cxx_flag_if_supported("-Wno-deprecated")
  add_cxx_flag_if_supported("-Wshorten-64-to-32")
  add_cxx_flag_if_supported("-Wnarrowing")
  if (ENABLE_WERROR)
    add_cxx_flag_if_supported("-Werror")
  endif ()
endif ()
