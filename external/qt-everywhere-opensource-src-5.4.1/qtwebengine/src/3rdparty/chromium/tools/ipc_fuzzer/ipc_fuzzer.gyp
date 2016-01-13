# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ipc_fuzzer',
      'type': 'none',
      'dependencies': [
        'mutate/mutate.gyp:ipc_fuzzer_mutate',
        'mutate/mutate.gyp:ipc_fuzzer_generate',
        'mutate/mutate.gyp:ipc_message_util',
        'replay/replay.gyp:ipc_fuzzer_replay',
      ],
    },
  ],
}
