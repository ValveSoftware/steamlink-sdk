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
    'ipc/client/command_buffer_proxy_impl.cc',
    'ipc/client/command_buffer_proxy_impl.h',
    'ipc/client/gpu_channel_host.cc',
    'ipc/client/gpu_channel_host.h',
    'ipc/client/gpu_memory_buffer_impl.cc',
    'ipc/client/gpu_memory_buffer_impl.h',
    'ipc/client/gpu_memory_buffer_impl_shared_memory.cc',
    'ipc/client/gpu_memory_buffer_impl_shared_memory.h',
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
    ['OS == "android"', {
      'sources+': [
        'ipc/client/gpu_memory_buffer_impl_surface_texture.cc',
        'ipc/client/gpu_memory_buffer_impl_surface_texture.h',
        'ipc/client/android/in_process_surface_texture_manager.cc',
        'ipc/client/android/in_process_surface_texture_manager.h',
      ],
    }],
    ['OS == "mac"', {
      'sources+': [
        'ipc/client/gpu_memory_buffer_impl_io_surface.cc',
        'ipc/client/gpu_memory_buffer_impl_io_surface.h',
        'ipc/client/gpu_process_hosted_ca_layer_tree_params.cc',
        'ipc/client/gpu_process_hosted_ca_layer_tree_params.h',
      ],
    }],
    ['use_ozone == 1', {
      'sources+': [
        'ipc/client/gpu_memory_buffer_impl_ozone_native_pixmap.cc',
        'ipc/client/gpu_memory_buffer_impl_ozone_native_pixmap.h',
      ],
      'dependencies': [
        '../ui/ozone/ozone.gyp:ozone_platform',
      ],
    }],
  ],
  # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
  'msvs_disabled_warnings': [4267, ],
}
