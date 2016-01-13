# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Build the specific library dependencies for validating x86 code, used
# by both the x86-32 and x86-64 validators.
# Note: Would like to name this file validator_x86.gyp, but that name is
# already used, and on mac's, this is not allowed.
{
  'includes': [
    '../../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="ncval_base"', {
        'sources': [
            'error_reporter.c',
            'halt_trim.c',
            'ncinstbuffer.c',
            'x86_insts.c',
            'nc_segment.c',
        ],
        'cflags!': [
          '-Wextra',
          '-Wswitch-enum',
          '-Wsign-compare'
        ],
        # VS2010 does not correctly incrementally link obj files generated
        # from asm files. This flag disables UseLibraryDependencyInputs to
        # avoid this problem.
        'msvs_2010_disable_uldi_when_referenced': 1,
        'xcode_settings': {
          'WARNING_CFLAGS!': [
            '-Wextra',
            '-Wswitch-enum',
            '-Wsign-compare'
          ],
        },
      }],
      ['target_base=="ncval_base_verbose"', {
        'sources': [
            'error_reporter_verbose.c',
            'x86_insts_verbose.c',
        ],
        'cflags!': [
          '-Wextra',
          '-Wswitch-enum',
          '-Wsign-compare'
        ],
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
        { 'target_name': 'ncval_base_x86_32',
          'type': 'static_library',
          'variables': {
            'target_base': 'ncval_base',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
            '<(DEPTH)/native_client/src/trusted/cpu_features/cpu_features.gyp:cpu_features',
            '<(DEPTH)/native_client/src/trusted/validator/validator.gyp:validation_cache',
          ],
        },
        { 'target_name': 'ncval_base_verbose_x86_32',
          'type': 'static_library',
          'variables': {
            'target_base': 'ncval_base_verbose',
          },
          'dependencies': [
            'ncval_base_x86_32',
          ],
        },
      ],
    }],
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        { 'target_name': 'ncval_base_x86_64',
          'type': 'static_library',
          'variables': {
            'target_base': 'ncval_base',
            'win_target': 'x64',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
            '<(DEPTH)/native_client/src/trusted/cpu_features/cpu_features.gyp:cpu_features64',
            '<(DEPTH)/native_client/src/trusted/validator/validator.gyp:validation_cache64',
          ],
        },
        { 'target_name': 'ncval_base_verbose_x86_64',
          'type': 'static_library',
          'variables': {
            'target_base': 'ncval_base_verbose',
            'win_target': 'x64',
          'dependencies': [
            'ncval_base_x86_64',
          ],
          },
        },
      ],
    }],
    ['target_arch=="x64"', {
      'targets': [
        { 'target_name': 'ncval_base_x86_64',
          'type': 'static_library',
          'variables': {
            'target_base': 'ncval_base',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
            '<(DEPTH)/native_client/src/trusted/cpu_features/cpu_features.gyp:cpu_features',
            '<(DEPTH)/native_client/src/trusted/validator/validator.gyp:validation_cache',
          ],
        },
        { 'target_name': 'ncval_base_verbose_x86_64',
          'type': 'static_library',
          'variables': {
            'target_base': 'ncval_base_verbose',
          },
          'dependencies': [
            'ncval_base_x86_64',
          ],
        },
      ],
    }],
  ],
}
