# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
     {
      'target_name': 'events_ipc',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/ipc/ipc.gyp:ipc',
      ],
      'defines': [
        'EVENTS_IMPLEMENTATION',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'latency_info_param_traits.cc',
        'latency_info_param_traits.h',
      ],
    },
  ],
}
