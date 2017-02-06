# -*- python -*-
#
# Copyright 2010 The Native Client Authors.  All rights reserved.  Use
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
      ['target_base=="nacl_base"', {
        'sources': [
          'nacl_refcount.h',
          'nacl_refcount.c',
        ],
        'xcode_settings': {
          'WARNING_CFLAGS': [
            '-fno-strict-aliasing',
            '-Wno-missing-field-initializers'
          ]
        },
      },
    ]],
  },
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'nacl_base64',
          'type': 'static_library',
          'variables': {
            'target_base': 'nacl_base',
            'win_target': 'x64',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/imc/imc.gyp:imc64',
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
          ],
        },
      ],
    }],
  ],
  'targets': [
    {
      'target_name': 'nacl_base',
      'type': 'static_library', # 'dynamic_library', ?!?
      'variables': {
        'target_base': 'nacl_base',
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
      ],
    },
  ],
}
