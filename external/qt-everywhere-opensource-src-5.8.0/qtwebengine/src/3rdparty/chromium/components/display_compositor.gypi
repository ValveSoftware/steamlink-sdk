# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/display_compositor:display_compositor
      'target_name': 'display_compositor',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../cc/cc.gyp:cc',
        '../gpu/gpu.gyp:command_buffer_client',
        '../gpu/gpu.gyp:command_buffer_common',
        '../skia/skia.gyp:skia',
        '../third_party/khronos/khronos.gyp:khronos_headers',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../ui/gl/gl.gyp:gl',
      ],

      'defines': [
        'DISPLAY_COMPOSITOR_IMPLEMENTATION',
      ],

      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],

      'include_dirs': [
        '..',
      ],

      'sources': [
        'display_compositor/buffer_queue.cc',
        'display_compositor/buffer_queue.h',
        'display_compositor/compositor_overlay_candidate_validator.h',
        'display_compositor/compositor_overlay_candidate_validator_android.cc',
        'display_compositor/compositor_overlay_candidate_validator_android.h',
        'display_compositor/compositor_overlay_candidate_validator_mac.h',
        'display_compositor/compositor_overlay_candidate_validator_mac.mm',
        'display_compositor/compositor_overlay_candidate_validator_ozone.cc',
        'display_compositor/compositor_overlay_candidate_validator_ozone.h',
        'display_compositor/display_compositor_export.h',
        'display_compositor/gl_helper.cc',
        'display_compositor/gl_helper.h',
        'display_compositor/gl_helper_readback_support.cc',
        'display_compositor/gl_helper_readback_support.h',
        'display_compositor/gl_helper_scaling.cc',
        'display_compositor/gl_helper_scaling.h',
      ],
      'conditions': [
        ['use_ozone==1', {
          'dependencies': [
            '../ui/ozone/ozone.gyp:ozone_base',
          ],
        }],
      ],
    },
    {
      # GN version: //components/display_compositor:display_compositor_gl_tests
      'target_name': 'display_compositor_gl_tests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'display_compositor',
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../cc/cc_tests.gyp:cc_test_support',
        '../gpu/gpu.gyp:command_buffer_client',
        '../media/media.gyp:media',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../ui/gl/gl.gyp:gl_test_support',
      ],

      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],

      'sources': [
        "display_compositor/display_compositor_test_suite.cc",
        "display_compositor/display_compositor_test_suite.h",
        "display_compositor/gl_helper_unittest.cc",
        "display_compositor/run_all_unittests.cc",
        "display_compositor/yuv_readback_unittest.cc",
      ]
    },
    {
      # GN version: //components/display_compositor:display_compositor_benchmark
      'target_name': 'display_compositor_benchmark',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'display_compositor',
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../cc/cc_tests.gyp:cc_test_support',
        '../gpu/gpu.gyp:command_buffer_client',
        '../media/media.gyp:media',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../ui/gl/gl.gyp:gl_test_support',
      ],

      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],

      'sources': [
        "display_compositor/display_compositor_test_suite.cc",
        "display_compositor/display_compositor_test_suite.h",
        "display_compositor/gl_helper_benchmark.cc",
        "display_compositor/run_all_unittests.cc",
      ]
    },
  ]
}
