# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ipc_fuzzer_all',
      'type': 'none',
      'dependencies': [
        'fuzzer/fuzzer.gyp:ipc_fuzzer',
        'message_dump/message_dump.gyp:ipc_message_dump',
        'message_replay/message_replay.gyp:ipc_fuzzer_replay',
        'message_tools/message_tools.gyp:ipc_message_list',
        'message_tools/message_tools.gyp:ipc_message_util',
      ],
    },
  ],
}
