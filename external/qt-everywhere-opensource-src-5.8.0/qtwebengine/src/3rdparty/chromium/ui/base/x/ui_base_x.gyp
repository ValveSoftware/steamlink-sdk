# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ui/base/x
      'target_name': 'ui_base_x',
      'type': '<(component)',
      'all_dependent_settings': {
        'ldflags': [
          '-L<(PRODUCT_DIR)',
        ],
      },
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/build/linux/system.gyp:x11',
        '<(DEPTH)/build/linux/system.gyp:xcursor',
        '<(DEPTH)/build/linux/system.gyp:xext',
        '<(DEPTH)/build/linux/system.gyp:xfixes',
        '<(DEPTH)/build/linux/system.gyp:xrender',  # For XRender* function calls in x11_util.cc.
        '<(DEPTH)/skia/skia.gyp:skia',
        '../../events/events.gyp:events',
        '../../events/devices/x11/events_devices_x11.gyp:events_devices_x11',
        '../../gfx/gfx.gyp:gfx',
        '../../gfx/gfx.gyp:gfx_geometry',
        '../../gfx/x/gfx_x11.gyp:gfx_x11',
      ],
      'defines': [
        'UI_BASE_X_IMPLEMENTATION',
      ],
      'sources': [
        'ui_base_x_export.h',
        'x11_foreign_window_manager.cc',
        'x11_foreign_window_manager.h',
        'x11_menu_list.cc',
        'x11_menu_list.h',
        'x11_util.cc',
        'x11_util.h',
        'x11_util_internal.h',
      ],
    },
  ],
}
