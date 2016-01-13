# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'gfx_unittests',
      'type': '<(gtest_target_type)',
      # iOS uses a small subset of ui. common_sources are the only files that
      # are built on iOS.
      'common_sources' : [
        'font_unittest.cc',
        'image/image_family_unittest.cc',
        'image/image_skia_unittest.cc',
        'image/image_unittest.cc',
        'image/image_unittest_util.cc',
        'image/image_unittest_util.h',
        'image/image_unittest_util_ios.mm',
        'image/image_unittest_util_mac.mm',
        'test/run_all_unittests.cc',
        'text_elider_unittest.cc',
        'text_utils_unittest.cc',
      ],
      'all_sources': [
        '<@(_common_sources)',
        'animation/animation_container_unittest.cc',
        'animation/animation_unittest.cc',
        'animation/multi_animation_unittest.cc',
        'animation/slide_animation_unittest.cc',
        'animation/tween_unittest.cc',
        'blit_unittest.cc',
        'break_list_unittest.cc',
        'canvas_unittest.cc',
        'codec/jpeg_codec_unittest.cc',
        'codec/png_codec_unittest.cc',
        'color_analysis_unittest.cc',
        'color_utils_unittest.cc',
        'display_unittest.cc',
        'font_list_unittest.cc',
        'geometry/box_unittest.cc',
        'geometry/cubic_bezier_unittest.cc',
        'geometry/insets_unittest.cc',
        'geometry/matrix3_unittest.cc',
        'geometry/point_unittest.cc',
        'geometry/point3_unittest.cc',
        'geometry/quad_unittest.cc',
        'geometry/r_tree_unittest.cc',
        'geometry/rect_unittest.cc',
        'geometry/safe_integer_conversions_unittest.cc',
        'geometry/size_unittest.cc',
        'geometry/vector2d_unittest.cc',
        'geometry/vector3d_unittest.cc',
        'image/image_mac_unittest.mm',
        'image/image_util_unittest.cc',
        'range/range_mac_unittest.mm',
        'range/range_unittest.cc',
        'range/range_win_unittest.cc',
        'sequential_id_generator_unittest.cc',
        'shadow_value_unittest.cc',
        'skbitmap_operations_unittest.cc',
        'skrect_conversion_unittest.cc',
        'transform_util_unittest.cc',
        'utf16_indexing_unittest.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../skia/skia.gyp:skia',
        '../../testing/gtest.gyp:gtest',
        '../../third_party/libpng/libpng.gyp:libpng',
        '../base/ui_base.gyp:ui_base',
        'gfx.gyp:gfx',
        'gfx.gyp:gfx_geometry',
        'gfx.gyp:gfx_test_support',
      ],
      'conditions': [
        ['OS == "ios"', {
          'sources': ['<@(_common_sources)'],
        }, {  # OS != "ios"
          'sources': ['<@(_all_sources)'],
        }],
        ['OS == "win"', {
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        }],
        ['OS != "mac" and OS != "ios"', {
          'sources': [
            'transform_unittest.cc',
            'interpolated_transform_unittest.cc',
          ],
        }],
        ['use_pango == 1', {
          'dependencies': [
            '../../build/linux/system.gyp:fontconfig',
            '../../build/linux/system.gyp:pangocairo',
          ],
          'sources': [
            'platform_font_pango_unittest.cc',
          ],
          'conditions': [
            ['use_allocator!="none"', {
              'dependencies': [
                '../../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
        ['use_ozone==1 and use_pango==0', {
          'sources!': [
            'canvas_unittest.cc',
            'font_list_unittest.cc',
            'font_unittest.cc',
            'text_elider_unittest.cc',
          ],
        }],
        ['OS == "android"', {
          'dependencies': [
            '../../testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
      ],
    }
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'gfx_unittests_apk',
          'type': 'none',
          'dependencies': [
            '../android/ui_android.gyp:ui_java',
            'gfx_unittests',
          ],
          'variables': {
            'test_suite_name': 'gfx_unittests',
          },
          'includes': [ '../../build/apk_test.gypi' ],
        },
      ],
    }],
  ],
}
