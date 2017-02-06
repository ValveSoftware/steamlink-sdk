# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ipc_message_util',
      'type': 'executable',
      'dependencies': [
        '../../../third_party/re2/re2.gyp:re2',
        '../message_lib/message_lib.gyp:ipc_message_lib',
      ],
      'sources': [
        'message_util.cc',
      ],
      'include_dirs': [
        '../../..',
      ],
      'defines': [
        'USE_CUPS',
      ],
    },
    {
      'target_name': 'ipc_message_list',
      'type': 'executable',
      'dependencies': [
        '../../../chrome/chrome.gyp:safe_browsing_proto',
        '../message_lib/message_lib.gyp:ipc_message_lib',
      ],
      'sources': [
        'message_list.cc',
      ]
    },
  ],
}
