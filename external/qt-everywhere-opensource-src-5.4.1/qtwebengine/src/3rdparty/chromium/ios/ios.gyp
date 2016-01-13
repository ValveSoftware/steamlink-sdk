# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
   },
  'targets': [
    {
      'target_name': 'ios',
      'type': 'none',
      'dependencies': [
        'ios_base.gyp:*',
        'ios_web.gyp:*',
        'ios_tests_unit.gyp:*',
      ],
    },
  ],
}
