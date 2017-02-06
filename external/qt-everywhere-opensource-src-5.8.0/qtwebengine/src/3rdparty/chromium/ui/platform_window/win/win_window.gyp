# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [{
    'target_name': 'win_window',
    'type': '<(component)',
    'dependencies': [
      '../../../base/base.gyp:base',
      '../../../skia/skia.gyp:skia',
      '../../events/events.gyp:events',
      '../../gfx/gfx.gyp:gfx',
      '../../gfx/gfx.gyp:gfx_geometry',
      '../platform_window.gyp:platform_window',
    ],
    'defines': [ 'WIN_WINDOW_IMPLEMENTATION' ],
    'sources': [
      'win_window.cc',
      'win_window.h',
      'win_window_export.h',
    ],
  }],
}
