# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Build the specific library dependencies for validating on x86-32
{
  'includes': [
    '../../../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="ncvalidate_x86_32"', {
        'sources': [
          'ncvalidate.c',
        ],
        'cflags!': [
          '-Wextra',
          '-Wswitch-enum',
          '-Wsign-compare'
        ],
        'defines': [ 'NACL_TRUSTED_BUT_NOT_TCB' ],
        'xcode_settings': {
          'WARNING_CFLAGS!': [
            '-Wextra',
            '-Wswitch-enum',
            '-Wsign-compare'
          ],
        },
      }],
      ['target_base=="ncvalidate_verbose_x86_32"', {
        'sources': [
          'ncvalidate_verbose.c',
        ],
        'cflags!': [
          '-Wextra',
          '-Wswitch-enum',
          '-Wsign-compare'
        ],
        'defines': [ 'NACL_TRUSTED_BUT_NOT_TCB' ],
        'xcode_settings': {
          'WARNING_CFLAGS!': [
            '-Wextra',
            '-Wswitch-enum',
            '-Wsign-compare'
          ],
        },
      }],
    ],
  },
  'conditions': [
    ['target_arch=="ia32"', {
      'targets': [
        # ----------------------------------------------------------------------
        {
          'target_name': 'ncvalidate_x86_32',
          'type': 'static_library',
          'variables': {
            'target_base': 'ncvalidate_x86_32',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/trusted/validator/x86/ncval_seg_sfi/ncval_seg_sfi.gyp:ncval_seg_sfi_x86_32',
          ],
          'hard_dependency': 1,
        },
        {
          'target_name': 'ncvalidate_verbose_x86_32',
          'type': 'static_library',
          'variables': {
            'target_base': 'ncvalidate_verbose_x86_32',
          },
          'dependencies': [
            'ncvalidate_x86_32',
            '<(DEPTH)/native_client/src/trusted/validator/x86/ncval_seg_sfi/ncval_seg_sfi.gyp:ncdis_seg_sfi_verbose_x86_32',
          ],
          'hard_dependency': 1,
        },
      ],
    }],
  ],
}

