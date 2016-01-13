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
      ['target_base=="thread_interface"', {
        'sources': [
          'nacl_thread_interface.h',
          'nacl_thread_interface.c',
        ],
        'xcode_settings': {
          'WARNING_CFLAGS': [
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
          'target_name': 'thread_interface64',
          'type': 'static_library',
          'variables': {
            'target_base': 'thread_interface',
            'win_target': 'x64',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
            '<(DEPTH)/native_client/src/trusted/nacl_base/nacl_base.gyp:nacl_base64',
          ],
        },
      ],
    }],
  ],
  'targets': [
    {
      'target_name': 'thread_interface',
      'type': 'static_library', # 'dynamic_library', ?!?
      'variables': {
        'target_base': 'thread_interface',
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/trusted/nacl_base/nacl_base.gyp:nacl_base',
      ],
    },
  ],
}
