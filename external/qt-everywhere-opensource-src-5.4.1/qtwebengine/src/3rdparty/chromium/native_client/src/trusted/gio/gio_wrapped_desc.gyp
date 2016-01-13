# -*- python -*-
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
      ['OS=="linux" or OS=="mac"', {
        'cflags': [
          '-fexceptions',
        ],
        'cflags_cc' : [
          '-frtti',
        ]
      }],
      ['OS=="mac"', {
        'xcode_settings': {
          'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',      # -fexceptions
          'GCC_ENABLE_CPP_RTTI': 'YES',            # -frtti
        }
      }],
     ],
   },
  'targets': [
    {
      'target_name': 'gio_wrapped_desc',
      'type': 'static_library',
      'sources': [
        'gio_shm.c',
        'gio_shm_unbounded.c',
        'gio_nacl_desc.c',
      ],
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer',
      ],
    },
  ],
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'gio_wrapped_desc64',
          'type': 'static_library',
          'variables': {
            'win_target': 'x64',
          },
          'sources': [
            'gio_shm.c',
            'gio_shm_unbounded.c',
            'gio_nacl_desc.c',
          ],
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio64',
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
            '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer64',
          ],
        },
     ],
    }],
  ]
}
