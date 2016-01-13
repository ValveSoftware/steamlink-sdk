# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="serialization"', {
        'sources': [
          'serialization.cc',
          'serialization.h',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'serialization',
      'type': 'static_library',
      'variables': {
        'target_base': 'serialization',
      },
    }
  ],
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'serialization64',
          'type': 'static_library',
          'variables': {
            'target_base': 'serialization',
            'win_target': 'x64'
          },
        }
      ]
    }],
  ],
}
