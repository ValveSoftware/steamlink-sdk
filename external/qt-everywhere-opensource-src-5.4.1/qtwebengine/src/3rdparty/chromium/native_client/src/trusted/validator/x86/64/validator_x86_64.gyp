# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Build the specific library dependencies for validating on x86-64
{
  'includes': [
    '../../../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="ncvalidate_x86_64"', {
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
      ['target_base=="ncvalidate_verbose_x86_64"', {
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
    'conditions': [
      ['OS=="win" and target_arch=="ia32"', {
        'variables': {
          'win_target': 'x64',
         },
      }],
    ],
  },
  'conditions': [
    ['OS=="win" or target_arch=="x64"', {
      'targets': [
        # ----------------------------------------------------------------------
        {
          'target_name': 'ncvalidate_x86_64',
          'type': 'static_library',
          'variables': {
            'target_base': 'ncvalidate_x86_64',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/trusted/validator/x86/ncval_reg_sfi/ncval_reg_sfi.gyp:ncval_reg_sfi_x86_64'
          ],
          'hard_dependency': 1,
        },
        {
          'target_name': 'ncvalidate_verbose_x86_64',
          'type': 'static_library',
          'variables': {
            'target_base': 'ncvalidate_verbose_x86_64',
          },
          'dependencies': [
            'ncvalidate_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator/x86/ncval_reg_sfi/ncval_reg_sfi.gyp:ncval_reg_sfi_verbose_x86_64'
          ],
          'hard_dependency': 1,
        },
      ],
    }],
  ],
}

