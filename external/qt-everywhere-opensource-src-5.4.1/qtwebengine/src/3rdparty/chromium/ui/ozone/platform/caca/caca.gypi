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
        '../../base/base.gyp:base',
        '../../skia/skia.gyp:skia',
        '../events/events.gyp:events',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
      ],
      'link_settings': {
        'libraries': [
          '-lcaca',
        ],
      },
      'sources': [
        'caca_connection.cc',
        'caca_connection.h',
        'caca_event_factory.cc',
        'caca_event_factory.h',
        'caca_surface_factory.cc',
        'caca_surface_factory.h',
        'ozone_platform_caca.cc',
        'ozone_platform_caca.h',
      ],
    },
  ],
}
