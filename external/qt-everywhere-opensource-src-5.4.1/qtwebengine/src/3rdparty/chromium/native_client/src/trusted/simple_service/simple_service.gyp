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
      ['target_base=="simple_service"', {
        'sources': [
          'nacl_simple_ltd_service.h',
          'nacl_simple_rservice.h',
          'nacl_simple_service.h',

          'nacl_simple_ltd_service.c',
          'nacl_simple_rservice.c',
          'nacl_simple_service.c',
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
          'target_name': 'simple_service64',
          'type': 'static_library',
          'variables': {
            'target_base': 'simple_service',
            'win_target': 'x64',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/trusted/threading/threading.gyp:thread_interface64',
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
            '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer64',
            '<(DEPTH)/native_client/src/trusted/nacl_base/nacl_base.gyp:nacl_base64',
          ],
        },
      ],
    }],
  ],
  'targets': [
    {
      'target_name': 'simple_service',
      'type': 'static_library', # 'dynamic_library', ?!?
      'variables': {
        'target_base': 'simple_service',
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/trusted/threading/threading.gyp:thread_interface',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer',
        '<(DEPTH)/native_client/src/trusted/nacl_base/nacl_base.gyp:nacl_base',
      ],
    },
  ],
}
