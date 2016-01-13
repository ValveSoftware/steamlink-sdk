# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'internal_ozone_platform_deps': [
      'ozone_platform_test',
    ],
    'internal_ozone_platforms': [
      'test'
    ],
  },
  'targets': [
    {
      'target_name': 'ozone_platform_test',
      'type': 'static_library',
      'defines': [
        'OZONE_IMPLEMENTATION',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../events/events.gyp:events',
        '../events/ozone/events_ozone.gyp:events_ozone_evdev',
        '../gfx/gfx.gyp:gfx',
      ],
      'sources': [
        'file_surface_factory.cc',
        'file_surface_factory.h',
        'ozone_platform_test.cc',
        'ozone_platform_test.h',
      ],
    },
  ],
}
