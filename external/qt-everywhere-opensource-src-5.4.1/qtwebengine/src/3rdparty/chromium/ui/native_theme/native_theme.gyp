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
        'fallback_theme.cc',
        'fallback_theme.h',
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
  ],
}
