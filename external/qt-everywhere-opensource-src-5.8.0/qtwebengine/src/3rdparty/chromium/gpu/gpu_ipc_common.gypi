# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'dependencies': [
    '../base/base.gyp:base',
    '../ipc/ipc.gyp:ipc',
    '../third_party/khronos/khronos.gyp:khronos_headers',
    '../ui/base/ui_base.gyp:ui_base',
    '../ui/events/events.gyp:events_base',
    '../ui/events/events.gyp:events_ipc',
    '../ui/gfx/gfx.gyp:gfx_geometry',
    '../ui/gfx/ipc/geometry/gfx_ipc_geometry.gyp:gfx_ipc_geometry',
    '../ui/gfx/ipc/gfx_ipc.gyp:gfx_ipc',
    '../ui/gl/gl.gyp:gl',
    '../ui/gl/init/gl_init.gyp:gl_init',
    '../url/url.gyp:url_lib',
    '../url/ipc/url_ipc.gyp:url_ipc',
  ],
  'include_dirs': [
    '..',
  ],
  'sources': [
    'ipc/common/gpu_memory_buffer_support.cc',
    'ipc/common/gpu_memory_buffer_support.h',
    'ipc/common/gpu_memory_uma_stats.h',
    'ipc/common/gpu_message_generator.cc',
    'ipc/common/gpu_message_generator.h',
    'ipc/common/gpu_messages.h',
    'ipc/common/gpu_param_traits.cc',
    'ipc/common/gpu_param_traits.h',
    'ipc/common/gpu_param_traits_macros.h',
    'ipc/common/gpu_stream_constants.h',
    'ipc/common/gpu_surface_lookup.cc',
    'ipc/common/gpu_surface_lookup.h',
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
    ['use_ozone==1', {
      'dependencies': [
        '../ui/ozone/ozone.gyp:ozone_platform',
      ],
    }],
  ],
}
