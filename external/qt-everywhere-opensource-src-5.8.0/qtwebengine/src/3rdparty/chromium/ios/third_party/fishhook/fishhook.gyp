# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //ios/third_party/fishhook
      'target_name': 'fishhook',
      'type': 'static_library',
      'sources': [
        'src/fishhook.c',
        'src/fishhook.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'src'
        ],
      },
    },
  ],
}
