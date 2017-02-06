# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/exo
      'target_name': 'exo',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../ash/ash.gyp:ash',
        '../base/base.gyp:base',
        '../cc/cc.gyp:cc',
        '../cc/cc.gyp:cc_surfaces',
        '../device/gamepad/gamepad.gyp:device_gamepad',
        '../gpu/gpu.gyp:gpu',
        '../skia/skia.gyp:skia',
        '../ui/aura/aura.gyp:aura',
        '../ui/compositor/compositor.gyp:compositor',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../ui/gl/gl.gyp:gl',
        '../ui/views/views.gyp:views',
        '../ui/wm/wm.gyp:wm',
      ],
      'export_dependent_settings': [
        '../ui/views/views.gyp:views',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'exo/buffer.cc',
        'exo/buffer.h',
        'exo/display.cc',
        'exo/display.h',
        'exo/gamepad.cc',
        'exo/gamepad.h',
        'exo/keyboard.cc',
        'exo/keyboard.h',
        'exo/keyboard_delegate.h',
        'exo/notification_surface.cc',
        'exo/notification_surface.h',
        'exo/notification_surface_manager.h',
        'exo/pointer.cc',
        'exo/pointer.h',
        'exo/pointer_delegate.h',
        'exo/shared_memory.cc',
        'exo/shared_memory.h',
        'exo/shell_surface.cc',
        'exo/shell_surface.h',
        'exo/sub_surface.cc',
        'exo/sub_surface.h',
        'exo/surface.cc',
        'exo/surface.h',
        'exo/surface_property.h',
        'exo/surface_delegate.h',
        'exo/surface_observer.h',
        'exo/touch.cc',
        'exo/touch.h',
        'exo/touch_delegate.h',
      ],
    },
  ],
  'conditions': [
    [ 'OS=="linux"', {
      'targets': [
        {
          # GN version: //components/exo:wayland
          'target_name': 'exo_wayland',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../ipc/ipc.gyp:ipc',
            '../skia/skia.gyp:skia',
            '../third_party/wayland-protocols/wayland-protocols.gyp:alpha_compositing_protocol',
            '../third_party/wayland-protocols/wayland-protocols.gyp:gaming_input_protocol',
            '../third_party/wayland-protocols/wayland-protocols.gyp:remote_shell_protocol',
            '../third_party/wayland-protocols/wayland-protocols.gyp:secure_output_protocol',
            '../third_party/wayland-protocols/wayland-protocols.gyp:stylus_protocol',
            '../third_party/wayland-protocols/wayland-protocols.gyp:viewporter_protocol',
            '../third_party/wayland-protocols/wayland-protocols.gyp:xdg_shell_protocol',
            '../third_party/wayland-protocols/wayland-protocols.gyp:vsync_feedback_protocol',
            '../third_party/wayland/wayland.gyp:wayland_server',
            '../ui/aura/aura.gyp:aura',
            '../ui/events/events.gyp:dom_keycode_converter',
            '../ui/display/display.gyp:display',
            '../ui/events/events.gyp:events_base',
            '../ui/views/views.gyp:views',
            'exo',
          ],
          'sources': [
            # Note: sources list duplicated in GN build.
            'exo/wayland/scoped_wl.cc',
            'exo/wayland/scoped_wl.h',
            'exo/wayland/server.cc',
            'exo/wayland/server.h',
          ],
          'conditions': [
            ['use_ozone==1', {
              'dependencies': [
                '../build/linux/system.gyp:libdrm',
                '../third_party/mesa/mesa.gyp:wayland_drm_protocol',
                '../third_party/wayland-protocols/wayland-protocols.gyp:linux_dmabuf_protocol',
              ],
            }],
            ['use_xkbcommon==1', {
              'dependencies': [
                '../build/linux/system.gyp:xkbcommon',
              ],
              'defines': [
                'USE_XKBCOMMON',
              ],
            }],
          ],
        },
      ],
    }],
  ],
}
