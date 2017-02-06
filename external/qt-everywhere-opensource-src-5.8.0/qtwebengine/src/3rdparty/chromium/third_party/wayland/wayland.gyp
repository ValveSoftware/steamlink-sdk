# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'wayland_util',
      'type': 'static_library',
      'sources': [
        'src/src/wayland-util.c',
        'src/src/wayland-util.h',
      ],
      'include_dirs': [
        'include/src',
        'include/protocol',
        'src/src',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include/src',
          'include/protocol',
          'src/src',
        ],
      },
    },
    {
      'target_name': 'wayland_private',
      'type': 'static_library',
      'dependencies': [
        '../../build/linux/system.gyp:libffi',
      ],
      'sources': [
        'src/src/connection.c',
        'src/src/wayland-os.c',
        'src/src/wayland-os.h',
        'src/src/wayland-private.h',
      ],
      'include_dirs': [
        'include/src',
        'include/protocol',
        'src/src',
      ],
    },
    {
      'target_name': 'wayland_protocol',
      'type': 'static_library',
      'dependencies': [
        'wayland_util',
      ],
      'sources': [
        'protocol/wayland-protocol.c',
      ],
      'include_dirs': [
        'include/src',
        'include/protocol',
        'src/src',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include/src',
          'include/protocol',
          'src/src',
        ],
      },
    },
    {
      'target_name': 'wayland_server',
      'type': 'static_library',
      'dependencies': [
        '../../build/linux/system.gyp:libffi',
        'wayland_private',
        'wayland_protocol',
        'wayland_util',
      ],
      'sources': [
        'include/protocol/wayland-server-protocol.h',
        'src/src/event-loop.c',
        'src/src/wayland-server.c',
        'src/src/wayland-shm.c',
      ],
      'include_dirs': [
        'include/src',
        'include/protocol',
        'src/src',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include/src',
          'include/protocol',
          'src/src',
        ],
      },
    },
    {
      'target_name': 'wayland_client',
      'type': 'static_library',
      'dependencies': [
        '../../build/linux/system.gyp:libffi',
        'wayland_private',
        'wayland_protocol',
        'wayland_util',
      ],
      'sources': [
        'include/protocol/wayland-client-protocol.h',
        'src/src/wayland-client.c',
      ],
      'include_dirs': [
        'include/src',
        'include/protocol',
        'src/src',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include/src',
          'include/protocol',
          'src/src',
        ],
      },
    },
  ],
}
