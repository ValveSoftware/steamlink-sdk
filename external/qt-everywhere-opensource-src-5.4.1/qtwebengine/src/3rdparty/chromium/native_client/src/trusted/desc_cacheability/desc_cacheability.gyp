# -*- python -*-
# Copyright 2011 (c) The Native Client Authors. All rights reserved.  Use
# of this source code is governed by a BSD-style license that can be
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
      ['target_base=="desc_cacheability"', {
        'sources': [
          'desc_cacheability.c',
          'desc_cacheability.h',
        ],
      }],
    ],
  },
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'desc_cacheability64',
          'type': 'static_library',
          'variables': {
            'target_base': 'desc_cacheability',
            'win_target': 'x64',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer64',
          ],
        },
      ],
    }],
  ],
  'targets': [
    {
      'target_name': 'desc_cacheability',
      'type': 'static_library',
      'variables': {
        'target_base': 'desc_cacheability',
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer',
      ],
    },
  ],
}
