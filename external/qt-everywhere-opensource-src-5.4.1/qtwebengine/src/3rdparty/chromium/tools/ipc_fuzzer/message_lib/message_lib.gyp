# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ipc_message_lib',
      'type': 'static_library',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../chrome/chrome.gyp:common',
        '../../../ipc/ipc.gyp:ipc',
        '../../../media/cast/cast.gyp:cast_transport',
        '../../../skia/skia.gyp:skia',
        '../../../third_party/libjingle/libjingle.gyp:libjingle',
        '../../../third_party/WebKit/public/blink.gyp:blink',
        '../../../ui/accessibility/accessibility.gyp:ax_gen',
      ],
      'sources': [
        'all_messages.h',
        'message_cracker.h',
        'message_file.h',
        'message_file_format.h',
        'message_file_reader.cc',
        'message_file_writer.cc',
        'message_names.cc',
        'message_names.h',
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
