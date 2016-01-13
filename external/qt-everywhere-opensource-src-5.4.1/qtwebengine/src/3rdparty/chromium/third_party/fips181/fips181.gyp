# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'fips181',
      'type': 'static_library',
      'include_dirs': [
        '.',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
      'sources': [
        'convert.cc',
        'convert.h',
        'fips181.cc',
        'fips181.h',
        'owntypes.h',
        'randpass.cc',
        'randpass.h',
        'smbl.h',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
      ],
    },
  ],
}
