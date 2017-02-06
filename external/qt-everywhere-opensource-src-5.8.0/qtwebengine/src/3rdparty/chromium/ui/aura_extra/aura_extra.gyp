# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ui/aura_extra
      'target_name': 'aura_extra',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../skia/skia.gyp:skia',
        '../aura/aura.gyp:aura',
        '../base/ui_base.gyp:ui_base',
        '../compositor/compositor.gyp:compositor',
        '../events/events.gyp:events',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
      ],
      'defines': [
        'AURA_EXTRA_IMPLEMENTATION',
      ],
      'sources': [
        'aura_extra_export.h',
        'image_window_delegate.cc',
        'image_window_delegate.h',
      ],
    },
  ],
}
