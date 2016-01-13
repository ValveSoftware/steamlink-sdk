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
      ['target_base=="generic_container"', {
        'sources': [
          'container.h',
          'container_list.c',
          'container_list.h',
          'container_hash_table.c',
          'container_hash_table.h',
        ],
      },
    ]],
  },
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'generic_container64',
          'type': 'static_library',
          'variables': {
            'target_base': 'generic_container',
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
      'target_name': 'generic_container',
      'type': 'static_library',
      'variables': {
        'target_base': 'generic_container',
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
      ],
    },
  ],
}
