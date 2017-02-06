# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ui_touch_selection',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../skia/skia.gyp:skia',
        '../base/ui_base.gyp:ui_base',
        '../events/events.gyp:events',
        '../events/events.gyp:gesture_detection',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
      ],
      'defines': [
        'UI_TOUCH_SELECTION_IMPLEMENTATION',
      ],
      'sources': [
        'longpress_drag_selector.cc',
        'longpress_drag_selector.h',
        'selection_event_type.h',
        'touch_handle.cc',
        'touch_handle.h',
        'touch_handle_orientation.h',
        'touch_selection_controller.cc',
        'touch_selection_controller.h',
        'touch_selection_draggable.h',
        'ui_touch_selection_export.h',
      ],
      'include_dirs': [
        '../..',
      ],
      'conditions': [
        ['use_aura==1', {
          'dependencies': [
            '../aura/aura.gyp:aura',
            '../aura_extra/aura_extra.gyp:aura_extra',
            '../compositor/compositor.gyp:compositor',
            '../gfx/gfx.gyp:gfx',
            '../resources/ui_resources.gyp:ui_resources',
          ],
          'sources': [
            'touch_handle_drawable_aura.cc',
            'touch_handle_drawable_aura.h',
            'touch_selection_menu_runner.cc',
            'touch_selection_menu_runner.h',
          ],
        }],
      ],
    },
    {
      'target_name': 'ui_touch_selection_test_support',
      'type': 'static_library',
      'dependencies': [
        'ui_touch_selection',
      ],
      'sources': [
        'touch_selection_controller_test_api.cc',
        'touch_selection_controller_test_api.h',
      ],
    },
    {
      'target_name': 'ui_touch_selection_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:run_all_unittests',
        '../../base/base.gyp:test_support_base',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        '../base/ui_base.gyp:ui_base',
        '../events/events.gyp:events_test_support',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_test_support',
        'ui_touch_selection',
        'ui_touch_selection_test_support',
      ],
      'sources': [
        'longpress_drag_selector_unittest.cc',
        'touch_handle_unittest.cc',
        'touch_selection_controller_unittest.cc',
      ],
      'include_dirs': [
        '../..',
      ],
      'conditions': [
        ['OS == "android"', {
          'dependencies': [
            '../../testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
      ]
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'selection_event_type_java',
          'type': 'none',
          'variables': {
            'source_file': 'selection_event_type.h',
          },
          'includes': [ '../../build/android/java_cpp_enum.gypi' ],
        },
        {
          'target_name': 'touch_handle_orientation_java',
          'type': 'none',
          'variables': {
            'source_file': 'touch_handle_orientation.h',
          },
          'includes': [ '../../build/android/java_cpp_enum.gypi' ],
        },
        {
          'target_name': 'ui_touch_selection_unittests_apk',
          'type': 'none',
          'dependencies': [
            'ui_touch_selection_unittests',
          ],
          'variables': {
            'test_suite_name': 'ui_touch_selection_unittests',
          },
          'includes': [ '../../build/apk_test.gypi' ],
        },
      ],
      'conditions': [
        ['test_isolation_mode != "noop"', {
          'targets': [
            {
              'target_name': 'ui_touch_selection_unittests_apk_run',
              'type': 'none',
              'dependencies': [
                'ui_touch_selection_unittests_apk',
              ],
              'includes': [
                '../../build/isolate.gypi',
              ],
              'sources': [
                'ui_touch_selection_unittests_apk.isolate',
              ],
            },
          ]
        }],
      ],
    }],  # OS == "android"
    ['test_isolation_mode != "noop" and OS != "android"', {
      'targets': [
        {
          'target_name': 'ui_touch_selection_unittests_run',
          'type': 'none',
          'dependencies': [
            'ui_touch_selection_unittests',
          ],
          'includes': [
            '../../build/isolate.gypi',
          ],
          'sources': [
            'ui_touch_selection_unittests.isolate',
          ],
          'conditions': [
            ['use_x11 == 1', {
              'dependencies': [
                '../../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
              ],
            }],
          ],
        },
      ],
    }],
  ],
}
