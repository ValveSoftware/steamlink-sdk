# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'xdg_shell_protocol',
      'type': 'static_library',
      'dependencies' : [
        '../wayland/wayland.gyp:wayland_util',
      ],
      'sources': [
        'include/protocol/xdg-shell-unstable-v5-client-protocol.h',
        'include/protocol/xdg-shell-unstable-v5-server-protocol.h',
        'protocol/xdg-shell-protocol.c',
      ],
      'include_dirs': [
        'include/protocol',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include/protocol',
        ],
      },
    },
    {
      'target_name': 'linux_dmabuf_protocol',
      'type': 'static_library',
      'dependencies' : [
        '../wayland/wayland.gyp:wayland_util',
      ],
      'sources': [
        'include/protocol/linux-dmabuf-unstable-v1-client-protocol.h',
        'include/protocol/linux-dmabuf-unstable-v1-server-protocol.h',
        'protocol/linux-dmabuf-protocol.c',
      ],
      'include_dirs': [
        'include/protocol',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include/protocol',
        ],
      },
    },
    {
      'target_name': 'viewporter_protocol',
      'type': 'static_library',
      'dependencies' : [
        '../wayland/wayland.gyp:wayland_util',
      ],
      'sources': [
        'include/protocol/viewporter-client-protocol.h',
        'include/protocol/viewporter-server-protocol.h',
        'protocol/viewporter-protocol.c',
      ],
      'include_dirs': [
        'include/protocol',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include/protocol',
        ],
      },
    },
    {
      'target_name': 'vsync_feedback_protocol',
      'type': 'static_library',
      'dependencies' : [
        '../wayland/wayland.gyp:wayland_util',
      ],
      'sources': [
        'include/protocol/vsync-feedback-unstable-v1-client-protocol.h',
        'include/protocol/vsync-feedback-unstable-v1-server-protocol.h',
        'protocol/vsync-feedback-protocol.c',
      ],
      'include_dirs': [
        'include/protocol',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include/protocol',
        ],
      },
    },
    {
      'target_name': 'secure_output_protocol',
      'type': 'static_library',
      'dependencies' : [
        '../wayland/wayland.gyp:wayland_util',
      ],
      'sources': [
        'include/protocol/secure-output-unstable-v1-client-protocol.h',
        'include/protocol/secure-output-unstable-v1-server-protocol.h',
        'protocol/secure-output-protocol.c',
      ],
      'include_dirs': [
        'include/protocol',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include/protocol',
        ],
      },
    },
    {
      'target_name': 'alpha_compositing_protocol',
      'type': 'static_library',
      'dependencies' : [
        '../wayland/wayland.gyp:wayland_util',
      ],
      'sources': [
        'include/protocol/alpha-compositing-unstable-v1-client-protocol.h',
        'include/protocol/alpha-compositing-unstable-v1-server-protocol.h',
        'protocol/alpha-compositing-protocol.c',
      ],
      'include_dirs': [
        'include/protocol',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include/protocol',
        ],
      },
    },
    {
      'target_name': 'remote_shell_protocol',
      'type': 'static_library',
      'dependencies' : [
        '../wayland/wayland.gyp:wayland_util',
      ],
      'sources': [
        'include/protocol/remote-shell-unstable-v1-client-protocol.h',
        'include/protocol/remote-shell-unstable-v1-server-protocol.h',
        'protocol/remote-shell-protocol.c',
      ],
      'include_dirs': [
        'include/protocol',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include/protocol',
        ],
      },
    },
    {
      'target_name': 'gaming_input_protocol',
      'type': 'static_library',
      'dependencies' : [
        '../wayland/wayland.gyp:wayland_util',
      ],
      'sources': [
        'include/protocol/gaming-input-unstable-v1-client-protocol.h',
        'include/protocol/gaming-input-unstable-v1-server-protocol.h',
        'protocol/gaming-input-protocol.c',
      ],
      'include_dirs': [
        'include/protocol',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include/protocol',
        ],
      },
    },
    {
      'target_name': 'stylus_protocol',
      'type': 'static_library',
      'dependencies' : [
        '../wayland/wayland.gyp:wayland_util',
      ],
      'sources': [
        'include/protocol/stylus-unstable-v1-client-protocol.h',
        'include/protocol/stylus-unstable-v1-server-protocol.h',
        'protocol/stylus-protocol.c',
      ],
      'include_dirs': [
        'include/protocol',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include/protocol',
        ],
      },
    },
  ],
}
