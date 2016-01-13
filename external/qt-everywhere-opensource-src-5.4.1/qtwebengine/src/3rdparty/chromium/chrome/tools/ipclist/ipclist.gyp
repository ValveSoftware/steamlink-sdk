# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ipclist',
      'type': 'executable',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../skia/skia.gyp:skia',
        '../../../third_party/khronos/khronos.gyp:khronos_headers',
      ],
      'sources': [
        'ipclist.cc',
      ]
    }
  ]
}
