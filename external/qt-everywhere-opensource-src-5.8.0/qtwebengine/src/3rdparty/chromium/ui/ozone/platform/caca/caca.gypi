# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'internal_ozone_platform_deps': [
      'ozone_platform_caca',
    ],
    'internal_ozone_platforms': [
      'caca'
    ],
  },
  'targets': [
    {
      'target_name': 'ozone_platform_caca',
      'type': 'static_library',
      'defines': [
        'OZONE_IMPLEMENTATION',
      ],
      'dependencies': [
        'ozone.gyp:ozone_base',
        'ozone.gyp:ozone_common',
        '../../base/base.gyp:base',
        '../../skia/skia.gyp:skia',
        '../events/events.gyp:events',
        '../events/ozone/events_ozone.gyp:events_ozone_layout',
        '../events/platform/events_platform.gyp:events_platform',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
      ],
      'link_settings': {
        'libraries': [
          '-lcaca',
        ],
      },
      'sources': [
        'caca_event_source.cc',
        'caca_event_source.h',
        'caca_window.cc',
        'caca_window.h',
        'caca_window_manager.cc',
        'caca_window_manager.h',
        'client_native_pixmap_factory_caca.cc',
        'client_native_pixmap_factory_caca.h',
        'ozone_platform_caca.cc',
        'ozone_platform_caca.h',
        'scoped_caca_types.cc',
        'scoped_caca_types.h',
      ],
    },
  ],
}
