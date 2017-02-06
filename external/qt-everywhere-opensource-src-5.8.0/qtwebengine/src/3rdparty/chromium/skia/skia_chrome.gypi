# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# This gypi file contains all the Chrome-specific enhancements to Skia.
# In component mode (shared_lib) it is folded into a single shared library with
# the Skia files but in all other cases it is a separate library.
{
  'dependencies': [
    'skia_library',
    '../base/base.gyp:base',
    '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
  ],

  'direct_dependent_settings': {
    'include_dirs': [
      'ext',
    ],
  },
  'variables': {
    # TODO(scottmg): http://crbug.com/177306
    'clang_warning_flags_unset': [
      # Don't warn about string->bool used in asserts.
      '-Wstring-conversion',
    ],
  },
  'sources': [
    # Note: file list duplicated in GN build.
    'ext/analysis_canvas.cc',
    'ext/benchmarking_canvas.cc',
    'ext/bitmap_platform_device_cairo.cc',
    'ext/bitmap_platform_device_mac.cc',
    'ext/bitmap_platform_device_skia.cc',
    'ext/bitmap_platform_device_win.cc',
    'ext/convolver.cc',
    'ext/event_tracer_impl.cc',
    'ext/fontmgr_default_win.cc',
    'ext/google_logging.cc',
    'ext/image_operations.cc',
    'ext/opacity_filter_canvas.cc',
    'ext/platform_canvas.cc',
    'ext/platform_device.cc',
    'ext/platform_device_linux.cc',
    'ext/platform_device_mac.cc',
    'ext/platform_device_win.cc',
    'ext/recursive_gaussian_convolution.cc',
    'ext/SkDiscardableMemory_chrome.cc',
    'ext/SkMemory_new_handler.cpp',
    'ext/skia_histogram.cc',
    'ext/skia_memory_dump_provider.cc',
    'ext/skia_trace_memory_dump_impl.cc',
    'ext/skia_utils_base.cc',
    'ext/skia_utils_ios.mm',
    'ext/skia_utils_mac.mm',
    'ext/skia_utils_win.cc',
  ],
  'conditions': [
    [ 'OS == "ios"', {
      'sources!': [
        'ext/platform_canvas.cc',
      ],
    }],
    [ 'OS == "android" and '
      'enable_basic_printing==0 and enable_print_preview==0', {
      'sources!': [
        'ext/skia_utils_base.cc',
      ],
    }],
    [ 'OS != "android" and (OS != "linux" or use_cairo==1)', {
      'sources!': [
        'ext/bitmap_platform_device_skia.cc',
      ],
    }],
    [ 'OS != "ios" and target_arch != "arm" and target_arch != "mipsel" and \
       target_arch != "arm64" and target_arch != "mips64el"', {
      'sources': [
        'ext/convolver_SSE2.cc',
      ],
    }],
    [ 'target_arch == "mipsel" and mips_dsp_rev >= 2',{
      'sources': [
        'ext/convolver_mips_dspr2.cc',
      ],
    }],
  ],

  'target_conditions': [
    # Pull in specific linux files for android (which have been filtered out
    # by file name rules).
    [ 'OS == "android" or qt_os == "embedded_linux"', {
      'sources/': [
        ['include', 'ext/platform_device_linux\\.cc$'],
      ],
    }],
  ],
}
