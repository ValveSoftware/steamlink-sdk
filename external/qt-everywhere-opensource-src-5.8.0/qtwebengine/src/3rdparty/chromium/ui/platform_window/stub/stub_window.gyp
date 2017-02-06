# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [{
    'target_name': 'stub_window',
    'type': '<(component)',
    'dependencies': [
      '../../../base/base.gyp:base',
      '../../gfx/gfx.gyp:gfx',
      '../../gfx/gfx.gyp:gfx_geometry',
      '../platform_window.gyp:platform_window',
    ],
    'defines': [ 'STUB_WINDOW_IMPLEMENTATION' ],
    'sources': [
      'stub_window.cc',
      'stub_window.h',
      'stub_window_export.h',
    ],
  }],
}
