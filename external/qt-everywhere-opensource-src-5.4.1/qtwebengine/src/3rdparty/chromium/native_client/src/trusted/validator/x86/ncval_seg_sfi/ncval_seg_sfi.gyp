# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(bradchen): eliminate need for the warning flag removals below
{
  'includes': [
    '../../../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="ncdis_seg_sfi"', {
        'sources': [
          'ncdecode.c',
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
      ['target_base=="ncdis_seg_sfi_verbose"', {
        'sources': [
          'ncdecode_verbose.c',
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
      ['target_base=="ncval_seg_sfi"', {
        'sources': [
          'ncvalidate.c',
          'ncvalidate_detailed.c',
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
  # ----------------------------------------------------------------------
  'conditions': [
    ['target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'ncdis_seg_sfi_x86_32',
          'type': 'static_library',
          'variables': {
            'target_base': 'ncdis_seg_sfi',
          },
        }, {
          'target_name': 'ncdis_seg_sfi_verbose_x86_32',
          'type': 'static_library',
          'variables': {
            'target_base': 'ncdis_seg_sfi_verbose',
          },
          'dependencies': [
            'ncdis_seg_sfi_x86_32',
            '<(DEPTH)/native_client/src/trusted/validator/x86/validate_x86.gyp:ncval_base_verbose_x86_32'
          ],
          'hard_dependency': 1,
        }, {
          'target_name': 'ncval_seg_sfi_x86_32',
          'type': 'static_library',
          'variables': {
            'target_base': 'ncval_seg_sfi',
          },
          'dependencies': [
            'ncdis_seg_sfi_x86_32',
            '<(DEPTH)/native_client/src/trusted/validator_x86/validator_x86.gyp:nccopy_x86_32',
            '<(DEPTH)/native_client/src/trusted/validator/x86/validate_x86.gyp:ncval_base_x86_32'
          ],
          'hard_dependency': 1,
          # VS2010 does not correctly incrementally link obj files generated
          # from asm files. This flag disables UseLibraryDependencyInputs to
          # avoid this problem.
          'msvs_2010_disable_uldi_when_referenced': 1,
        }],
    }],
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'ncdis_seg_sfi_x86_64',
          'type': 'static_library',
          'variables': {
            'target_base': 'ncdis_seg_sfi',
            'win_target': 'x64',
          },
        }, {
          'target_name': 'ncdis_seg_sfi_verbose_x86_64',
          'type': 'static_library',
          'variables': {
            'target_base': 'ncdis_seg_sfi_verbose',
            'win_target': 'x64',
          },
          'dependencies': [
            'ncval_seg_sfi_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator/x86/validate_x86.gyp:ncval_base_verbose_x86_64'
          ],
          'hard_dependency': 1,
        }, {
          'target_name': 'ncval_seg_sfi_x86_64',
          'type': 'static_library',
          'variables': {
            'target_base': 'ncval_seg_sfi',
            'win_target': 'x64',
          },
          'dependencies': [
            'ncdis_seg_sfi_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator_x86/validator_x86.gyp:nccopy_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator/x86/validate_x86.gyp:ncval_base_x86_64'
          ],
          'hard_dependency': 1,
        }],
    }],
    ['target_arch=="x64"', {
      'targets': [
        {
          'target_name': 'ncdis_seg_sfi_x86_64',
          'type': 'static_library',
          'variables': {
            'target_base': 'ncdis_seg_sfi',
          },
        }, {
          'target_name': 'ncdis_seg_sfi_verbose_x86_64',
          'type': 'static_library',
          'variables': {
            'target_base': 'ncdis_seg_sfi_verbose',
          },
          'dependencies': [
            'ncval_seg_sfi_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator/x86/validate_x86.gyp:ncval_base_verbose_x86_64'          ],
          'hard_dependency': 1,
        }, {
          'target_name': 'ncval_seg_sfi_x86_64',
          'type': 'static_library',
          'variables': {
            'target_base': 'ncval_seg_sfi',
          },
          'dependencies': [
            'ncdis_seg_sfi_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator_x86/validator_x86.gyp:nccopy_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator/x86/validate_x86.gyp:ncval_base_x86_64'          ],
          'hard_dependency': 1,
        }],
    }],
  ],
}
