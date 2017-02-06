# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'internal_ozone_platform_deps': [
      'ozone_platform_headless',
    ],
    'internal_ozone_platforms': [
      'headless'
    ],
  },
  'targets': [
    {
      'target_name': 'ozone_platform_headless',
      'type': 'static_library',
      'defines': [
        'OZONE_IMPLEMENTATION',
      ],
      'dependencies': [
        'ozone.gyp:ozone_base',
        'ozone.gyp:ozone_common',
        '../../base/base.gyp:base',
        '../base/ui_base.gyp:ui_base',
        '../events/events.gyp:events',
        '../events/ozone/events_ozone.gyp:events_ozone_layout',
        '../events/platform/events_platform.gyp:events_platform',
        '../gfx/gfx.gyp:gfx',
      ],
      'sources': [
        'client_native_pixmap_factory_headless.cc',
        'client_native_pixmap_factory_headless.h',
        'ozone_platform_headless.cc',
        'ozone_platform_headless.h',
        'headless_surface_factory.cc',
        'headless_surface_factory.h',
        'headless_window.cc',
        'headless_window.h',
        'headless_window_manager.cc',
        'headless_window_manager.h',
      ],
    },
  ],
}
