# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ipc_fuzzer_replay',
      'type': 'executable',
      'dependencies': [
        '../message_lib/message_lib.gyp:ipc_message_lib',
        '../../../base/base.gyp:base',
        '../../../chrome/chrome.gyp:common',
        '../../../ipc/ipc.gyp:ipc',
        '../../../media/cast/cast.gyp:cast_transport',
      ],
      'sources': [
        'replay.cc',
        'replay_process.cc',
        'replay_process.h',
      ],
      'include_dirs': [
        '../../..',
      ],
      'defines': [
        'USE_CUPS',
      ],
    },
  ],
}
