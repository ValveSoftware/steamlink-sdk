# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ipc_message_dump',
      'type': 'loadable_module',
      'dependencies': [
        '../message_lib/message_lib.gyp:ipc_message_lib',
      ],
      'sources': [
        'message_dump.cc',
      ],
      'include_dirs': [
        '../../..',
      ],
    },
  ],
}
