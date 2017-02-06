# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'native_theme',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../skia/skia.gyp:skia',
        '../base/ui_base.gyp:ui_base',
        '../display/display.gyp:display',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        '../resources/ui_resources.gyp:ui_resources',
      ],
      'defines': [
        'NATIVE_THEME_IMPLEMENTATION',
      ],
      'sources': [
        'common_theme.cc',
        'common_theme.h',
        'native_theme.cc',
        'native_theme.h',
        'native_theme_android.cc',
        'native_theme_android.h',
        'native_theme_aura.cc',
        'native_theme_aura.h',
        'native_theme_aurawin.cc',
        'native_theme_aurawin.h',
        'native_theme_base.cc',
        'native_theme_base.h',
        'native_theme_dark_aura.cc',
        'native_theme_dark_aura.h',
        'native_theme_dark_win.cc',
        'native_theme_dark_win.h',
        'native_theme_mac.h',
        'native_theme_mac.mm',
        'native_theme_observer.cc',
        'native_theme_observer.h',
        'native_theme_switches.cc',
        'native_theme_switches.h',
        'native_theme_win.cc',
        'native_theme_win.h',
      ],
    },
    {
      'target_name': 'native_theme_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../skia/skia.gyp:skia',
        '../../testing/gtest.gyp:gtest',
        '../gfx/gfx.gyp:gfx_geometry',
        'native_theme',
      ],
      'sources': [
        '../../base/test/run_all_unittests.cc',
        'native_theme_aura_unittest.cc',
        'native_theme_mac_unittest.cc',
      ],
    },
  ],
}
