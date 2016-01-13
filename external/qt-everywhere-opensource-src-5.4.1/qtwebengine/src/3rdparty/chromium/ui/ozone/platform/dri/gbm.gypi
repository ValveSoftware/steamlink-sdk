# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'internal_ozone_platform_deps': [
      'ozone_platform_gbm',
    ],
    'internal_ozone_platforms': [
      'gbm',
    ],
  },
  'targets': [
    {
      'target_name': 'ozone_platform_gbm',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../build/linux/system.gyp:dridrm',
        '../../build/linux/system.gyp:gbm',
        '../../skia/skia.gyp:skia',
        '../base/ui_base.gyp:ui_base',
        '../events/events.gyp:events',
        '../events/ozone/events_ozone.gyp:events_ozone',
        '../gfx/gfx.gyp:gfx',
      ],
      'defines': [
        'OZONE_IMPLEMENTATION',
      ],
      'sources': [
        'buffer_data.cc',
        'buffer_data.h',
        'gbm_surface.cc',
        'gbm_surface.h',
        'gbm_surface_factory.cc',
        'gbm_surface_factory.h',
        'ozone_platform_gbm.cc',
        'ozone_platform_gbm.h',
      ],
    },
  ],
}
