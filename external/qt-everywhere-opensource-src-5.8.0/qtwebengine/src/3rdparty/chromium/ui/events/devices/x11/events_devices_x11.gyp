# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'events_devices_x11',
      'type': '<(component)',
      'dependencies': [
        '../../../../base/base.gyp:base',
        '../../../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../../../build/linux/system.gyp:x11',
        '../../../../skia/skia.gyp:skia',
        '../../../../ui/events/events.gyp:events_base',
        '../../../../ui/events/devices/events_devices.gyp:events_devices',
        '../../../../ui/events/keycodes/events_keycodes.gyp:keycodes_x11',
        '../../../../ui/gfx/gfx.gyp:gfx',
        '../../../../ui/gfx/gfx.gyp:gfx_geometry',
        '../../../../ui/gfx/x/gfx_x11.gyp:gfx_x11',
      ],
      'defines': [
        'EVENTS_DEVICES_X11_IMPLEMENTATION',
      ],
      'sources': [
        'device_data_manager_x11.cc',
        'device_data_manager_x11.h',
        'device_list_cache_x11.cc',
        'device_list_cache_x11.h',
        'events_devices_x11_export.h',
        'touch_factory_x11.cc',
        'touch_factory_x11.h',
      ],
    },
  ],
}
