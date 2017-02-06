# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'dependencies': [
    '../base/base.gyp:base',
    '../ipc/ipc.gyp:ipc',
    '../third_party/khronos/khronos.gyp:khronos_headers',
  ],
  'include_dirs': [
    '..',
  ],
  'sources': [
    'ipc/common/gpu_command_buffer_traits.cc',
    'ipc/common/gpu_command_buffer_traits.h',
    'ipc/common/memory_stats.cc',
    'ipc/common/memory_stats.h',
    'ipc/common/surface_handle.h',
  ],
  'conditions': [
    # This section applies to gpu_ipc_win64, used by the NaCl Win64 helper
    # (nacl64.exe).
    ['nacl_win64_target==1', {
      # gpu_ipc_win64 must only link against the 64-bit ipc target.
      'dependencies!': [
        '../base/base.gyp:base',
        '../ipc/ipc.gyp:ipc',
      ],
    }],
    ['qt_os=="mac" or qt_os=="win32"', {
        'export_dependent_settings': [
          '../third_party/khronos/khronos.gyp:khronos_headers',
        ],
    }],
    ['OS=="android"', {
      'sources': [
        'ipc/common/android/surface_texture_manager.cc',
        'ipc/common/android/surface_texture_manager.h',
        'ipc/common/android/surface_texture_peer.cc',
        'ipc/common/android/surface_texture_peer.h',
      ]
    }]
  ],
}
