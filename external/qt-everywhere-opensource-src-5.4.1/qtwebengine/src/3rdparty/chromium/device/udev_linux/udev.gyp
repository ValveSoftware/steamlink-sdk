# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'conditions': [
    ['use_udev==1', {
      'targets': [
        {
          'target_name': 'udev_linux',
          'type': 'static_library',
          'dependencies': [
            '../../base/base.gyp:base',
            '../../build/linux/system.gyp:udev',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'udev.cc',
            'udev.h',
          ],
        },
      ],
    }],
  ]
}
