# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
   },
  'targets': [
    {
      'target_name': 'ios_consumer_base',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'consumer/base/supports_user_data.cc',
        'consumer/base/util.mm',
        'public/consumer/base/supports_user_data.h',
        'public/consumer/base/util.h',
      ],
    },
  ],
}
