# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'disable_display%': 0,
    'internal_ozone_platform_deps': [
      'ozone_platform_cast',
    ],
    'internal_ozone_platform_unittest_deps': [ ],
    'internal_ozone_platforms': [
      'cast',
    ],
  },
  'targets': [
    # GN target: //ui/ozone/platform/cast:cast
    {
      'target_name': 'ozone_platform_cast',
      'type': 'static_library',
      'dependencies': [
        'ozone.gyp:ozone_base',
        'ozone.gyp:ozone_common',
        '../events/events.gyp:events',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        '../../base/base.gyp:base',
        '../../chromecast/chromecast.gyp:cast_public_api',
        '../../chromecast/chromecast.gyp:libcast_graphics_1.0',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/khronos',
      ],
      'conditions': [
        ['disable_display==1', {
          'defines': ['DISABLE_DISPLAY'],
        }],
      ],

      'sources': [
        'client_native_pixmap_factory_cast.cc',
        'client_native_pixmap_factory_cast.h',
        'gpu_platform_support_cast.cc',
        'gpu_platform_support_cast.h',
        'overlay_manager_cast.cc',
        'overlay_manager_cast.h',
        'ozone_platform_cast.cc',
        'ozone_platform_cast.h',
        'platform_window_cast.cc',
        'platform_window_cast.h',
        'surface_factory_cast.cc',
        'surface_factory_cast.h',
        'surface_ozone_egl_cast.cc',
        'surface_ozone_egl_cast.h',
      ],
      'link_settings': {
        'libraries': [
          '-ldl',
        ],
      },
    },
  ],
}
