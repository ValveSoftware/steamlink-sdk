# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ios/testing/earl_grey:earl_grey_support
      'target_name': 'earl_grey_support',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/ios/third_party/earl_grey/earl_grey.gyp:EarlGrey',
      ],
      'sources': [
        'wait_util.h',
        'wait_util.mm',
      ],
      'include_dirs': [
        '../../..',
      ],
    },
  ],
}
