# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //media/base/ipc
      'target_name': 'media_base_ipc',
      'type': 'static_library',
      'dependencies': [
        '../../media.gyp:media',
        '../../../base/base.gyp:base',
        '../../../ipc/ipc.gyp:ipc',
        '../../../ui/gfx/ipc/geometry/gfx_ipc_geometry.gyp:gfx_ipc_geometry',
        '../../../ui/gfx/ipc/gfx_ipc.gyp:gfx_ipc',
      ],
      # This sources list is duplicated in //media/base/ipc/BUILD.gn
      'sources': [
        'media_param_traits.h',
        'media_param_traits.cc',
        'media_param_traits_macros.h',
      ],
    }
  ]
}
