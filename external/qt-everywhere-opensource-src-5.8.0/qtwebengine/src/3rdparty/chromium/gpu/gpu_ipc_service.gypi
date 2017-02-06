# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'dependencies': [
    '../base/base.gyp:base',
    '../ipc/ipc.gyp:ipc',
    '../ui/events/events.gyp:events_base',
    '../ui/events/events.gyp:events_ipc',
    '../ui/gfx/gfx.gyp:gfx',
    '../ui/gfx/gfx.gyp:gfx_geometry',
    '../ui/gl/gl.gyp:gl',
    '../ui/gl/init/gl_init.gyp:gl_init',
    '../url/url.gyp:url_lib',
    '../third_party/khronos/khronos.gyp:khronos_headers',
  ],
  'include_dirs': [
    '..',
  ],
  'sources': [
      'ipc/service/gpu_channel.cc',
      'ipc/service/gpu_channel.h',
      'ipc/service/gpu_channel_manager.cc',
      'ipc/service/gpu_channel_manager.h',
      'ipc/service/gpu_channel_manager_delegate.h',
      'ipc/service/gpu_command_buffer_stub.cc',
      'ipc/service/gpu_command_buffer_stub.h',
      'ipc/service/gpu_config.h',
      'ipc/service/gpu_memory_buffer_factory.cc',
      'ipc/service/gpu_memory_buffer_factory.h',
      'ipc/service/gpu_memory_manager.cc',
      'ipc/service/gpu_memory_manager.h',
      'ipc/service/gpu_memory_tracking.cc',
      'ipc/service/gpu_memory_tracking.h',
      'ipc/service/gpu_watchdog.h',
      'ipc/service/image_transport_surface.h',
      'ipc/service/pass_through_image_transport_surface.cc',
      'ipc/service/pass_through_image_transport_surface.h',
  ],
  'conditions': [
    ['OS=="win"', {
      'sources': [
        'ipc/service/child_window_surface_win.cc',
        'ipc/service/child_window_surface_win.h',
        'ipc/service/image_transport_surface_win.cc',
      ],
    }],
    ['OS=="mac"', {
      'sources': [
        'ipc/service/image_transport_surface_overlay_mac.h',
        'ipc/service/image_transport_surface_overlay_mac.mm',
        'ipc/service/gpu_memory_buffer_factory_io_surface.cc',
        'ipc/service/gpu_memory_buffer_factory_io_surface.h',
        'ipc/service/image_transport_surface_mac.mm',
      ],
      'dependencies': [
        '../ui/accelerated_widget_mac/accelerated_widget_mac.gyp:accelerated_widget_mac',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
        ],
      },
    }],
    ['OS=="android"', {
      'sources': [
        'ipc/service/image_transport_surface_android.cc',
        'ipc/service/stream_texture_android.cc',
        'ipc/service/stream_texture_android.h',
        'ipc/service/gpu_memory_buffer_factory_surface_texture.cc',
        'ipc/service/gpu_memory_buffer_factory_surface_texture.h',
      ],
      'link_settings': {
        'libraries': [
          '-landroid',  # ANativeWindow
        ],
      },
    }],
    ['OS=="linux"', {
      'sources': [ 'ipc/service/image_transport_surface_linux.cc' ],
    }],
    ['use_x11 == 1 and (target_arch != "arm" or chromeos == 0)', {
      'sources': [
        'ipc/service/x_util.h',
      ],
    }],
    ['use_ozone == 1', {
      'sources': [
        'ipc/service/gpu_memory_buffer_factory_ozone_native_pixmap.cc',
        'ipc/service/gpu_memory_buffer_factory_ozone_native_pixmap.h',
      ],
      'dependencies': [
        '../ui/ozone/ozone.gyp:ozone',
      ],
    }],
  ],
}
