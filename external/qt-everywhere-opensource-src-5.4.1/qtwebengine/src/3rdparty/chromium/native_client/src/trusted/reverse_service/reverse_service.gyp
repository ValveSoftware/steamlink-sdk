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
      ['target_base=="reverse_service"', {
        'sources': [
          'reverse_service_c.h',
          'reverse_service.h',
          'reverse_service_c.c',
          'reverse_service.cc',
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
          'target_name': 'reverse_service64',
          'type': 'static_library',
          'variables': {
            'target_base': 'reverse_service',
            'win_target': 'x64',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
            '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc64',
            '<(DEPTH)/native_client/src/trusted/threading/threading.gyp:thread_interface64',
            '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:desc_wrapper64',
            '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer64',
            '<(DEPTH)/native_client/src/trusted/nacl_base/nacl_base.gyp:nacl_base64',
            '<(DEPTH)/native_client/src/trusted/validator/validator.gyp:validation_cache64',
          ],
        },
      ],
    }],
  ],
  'targets': [
    {
      'target_name': 'reverse_service',
      'type': 'static_library', # 'dynamic_library', ?!?
      'variables': {
        'target_base': 'reverse_service',
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc',
        '<(DEPTH)/native_client/src/trusted/threading/threading.gyp:thread_interface',
        '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:desc_wrapper',
        '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer',
        '<(DEPTH)/native_client/src/trusted/nacl_base/nacl_base.gyp:nacl_base',
        '<(DEPTH)/native_client/src/trusted/simple_service/simple_service.gyp:simple_service',
        '<(DEPTH)/native_client/src/trusted/validator/validator.gyp:validation_cache',
      ],
    },
  ],
}
