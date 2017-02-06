# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ui/gfx/ipc/skia
      'target_name': 'gfx_ipc_skia',
      'type': '<(component)',
      'dependencies': [
        '../../../../base/base.gyp:base',
        '../../../../ipc/ipc.gyp:ipc',
        '../../../../skia/skia.gyp:skia',
        '../../gfx.gyp:gfx',
        '../../gfx.gyp:gfx_geometry',
        '../gfx_ipc.gyp:gfx_ipc',
      ],
      'defines': [
        'GFX_SKIA_IPC_IMPLEMENTATION',
      ],
      'include_dirs': [
        '../../../..',
      ],
      'sources': [
        'gfx_skia_param_traits.cc',
        'gfx_skia_param_traits.h',
      ],
    },
  ],
}
