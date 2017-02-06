# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'input_tools_root_dir': '../../third_party/google_input_tools/src/chrome/os',
    'inputview_gen_js': '<(SHARED_INTERMEDIATE_DIR)/ui/keyboard/resources/inputview.js',
  },
  'targets': [
    {
      # GN version: //ui/keyboard:resources
      'target_name': 'keyboard_resources',
      'dependencies': [
        '../../third_party/google_input_tools/inputview.gyp:inputview',
       ],
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ui/keyboard',
      },
      'actions': [
        {
          'action_name': 'keyboard_resources',
          'variables': {
            'grit_grd_file': 'keyboard_resources.grd',
            'grit_additional_defines': [
              '-E', 'input_tools_root_dir=<(input_tools_root_dir)',
              '-E', 'inputview_gen_js=<(inputview_gen_js)',
            ],
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../build/grit_target.gypi' ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [
            '<(SHARED_INTERMEDIATE_DIR)/ui/keyboard/keyboard_resources.pak',
          ],
        },
      ],
    },
    {
      # GN version: //ui/keyboard
      'target_name': 'keyboard',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../media/media.gyp:media',
        '../../skia/skia.gyp:skia',
        '../aura/aura.gyp:aura',
        '../base/ime/ui_base_ime.gyp:ui_base_ime',
        '../base/ui_base.gyp:ui_base',
        '../compositor/compositor.gyp:compositor',
        '../events/events.gyp:dom_keycode_converter',
        '../events/events.gyp:events',
        '../events/events.gyp:events_base',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        'keyboard_resources',
      ],
      'defines': [
        'KEYBOARD_IMPLEMENTATION',
      ],
      'sources': [
        'keyboard_controller.cc',
        'keyboard_controller.h',
        'keyboard_controller_observer.h',
        'keyboard_export.h',
        'keyboard_layout_manager.cc',
        'keyboard_layout_manager.h',
        'keyboard_switches.cc',
        'keyboard_switches.h',
        'keyboard_ui.cc',
        'keyboard_ui.h',
        'keyboard_util.cc',
        'keyboard_util.h',
      ],
      'conditions': [
        ['use_ozone==1', {
          'dependencies': [
            '../ozone/ozone.gyp:ozone',
          ],
        }],
      ],
    },
    {
      # GN version: //ui/keyboard:keyboard_with_content
      'target_name': 'keyboard_with_content',
      'type': '<(component)',
      'dependencies': [
        'keyboard',
        '../../base/base.gyp:base',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../content/content.gyp:content_browser',
        '../../ipc/ipc.gyp:ipc',
        '../../skia/skia.gyp:skia',
        '../../url/url.gyp:url_lib',
        '../aura/aura.gyp:aura',
        '../base/ime/ui_base_ime.gyp:ui_base_ime',
        '../base/ui_base.gyp:ui_base',
        '../compositor/compositor.gyp:compositor',
        '../events/events.gyp:dom_keycode_converter',
        '../events/events.gyp:events',
        '../events/events.gyp:events_base',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        '../wm/wm.gyp:wm',
        'keyboard_resources',
      ],
      'defines': [
        'KEYBOARD_IMPLEMENTATION',
      ],
      'sources': [
        'content/keyboard.cc',
        'content/keyboard.h',
        'content/keyboard_constants.cc',
        'content/keyboard_constants.h',
        'content/keyboard_content_util.cc',
        'content/keyboard_content_util.h',
        'content/keyboard_ui_content.cc',
        'content/keyboard_ui_content.h',
      ],
    },
    {
      'target_name': 'keyboard_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../skia/skia.gyp:skia',
        '../../testing/gtest.gyp:gtest',
        '../aura/aura.gyp:aura',
        '../aura/aura.gyp:aura_test_support',
        '../base/ime/ui_base_ime.gyp:ui_base_ime',
        '../base/ui_base.gyp:ui_base',
        '../compositor/compositor.gyp:compositor',
        '../compositor/compositor.gyp:compositor_test_support',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        '../wm/wm.gyp:wm',
        'keyboard',
      ],
      'sources': [
        'keyboard_controller_unittest.cc',
        'keyboard_util_unittest.cc',
        'test/run_all_unittests.cc',
      ],
    },
  ],
}
