# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'use_x11': 1,
  },
  'targets': [{
    # GN version: //ui/platform_window/x11
    'target_name': 'x11_window',
    'type': '<(component)',
    'dependencies': [
      '../../../base/base.gyp:base',
      '../../../build/linux/system.gyp:x11',
      '../../../skia/skia.gyp:skia',
      '../../events/devices/events_devices.gyp:events_devices',
      '../../events/events.gyp:events',
      '../../events/devices/x11/events_devices_x11.gyp:events_devices_x11',
      '../../events/platform/events_platform.gyp:events_platform',
      '../../events/platform/x11/x11_events_platform.gyp:x11_events_platform',
      '../../gfx/x/gfx_x11.gyp:gfx_x11',
      '../platform_window.gyp:platform_window',
    ],
    'defines': [ 'X11_WINDOW_IMPLEMENTATION' ],
    'sources': [
      'x11_window.cc',
      'x11_window.h',
      'x11_window_base.cc',
      'x11_window_base.h',
      'x11_window_export.h',
    ],
  }],
}
