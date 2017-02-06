# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
   },
  'targets': [
    {
      # GN version: //ios/public/consumer/base
      'target_name': 'ios_consumer_base',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        '../public/consumer/base/debugger.h',
        'base/debugger.mm',
      ],
    },
  ],
}
