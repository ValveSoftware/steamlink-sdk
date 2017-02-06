# -*- python -*-
#
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables':{
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="nacl_interval"', {
        'sources': [
          'nacl_interval_multiset_delete.c',
          'nacl_interval_multiset_factory.c',
          'nacl_interval_multiset.h',
          'nacl_interval_list.c',
          'nacl_interval_list.h',
          'nacl_interval_range_tree.c',
          'nacl_interval_range_tree.h',
        ],
      },
    ]],
  },
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'nacl_interval64',
          'type': 'static_library',
          'variables': {
            'target_base': 'nacl_interval',
            'win_target': 'x64',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
          ],
        },
      ],
    }],
  ],
  'targets': [
    {
      'target_name': 'nacl_interval',
      'type': 'static_library',
      'variables': {
        'target_base': 'nacl_interval',
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
      ],
    },
  ],
}
