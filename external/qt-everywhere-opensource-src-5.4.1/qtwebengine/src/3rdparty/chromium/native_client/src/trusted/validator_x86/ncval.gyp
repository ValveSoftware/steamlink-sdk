# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(bradchen): eliminate need for the warning flag removals below
{
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="ncdis_util"', {
        'sources': [
          'ncdis_segments.c',
          'nc_read_segment.c',
        ],
        'defines': [
          'NACL_TRUSTED_BUT_NOT_TCB',
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
      ['target_base=="ncval"', {
        'sources': [
          'ncval.c',
        ],
        'defines': [
          'NACL_TRUSTED_BUT_NOT_TCB',
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
          'target_name': 'ncdis_util_x86_32',
          'type': 'static_library',
          'variables': {
            'target_base': 'ncdis_util',
          },
          'sources': ['ncenuminsts_x86_32.c'],
        }, {
          'target_name': 'ncval_x86_32',
          'type': 'executable',
          'variables': {
            'target_base': 'ncval',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/utils/utils.gyp:utils',
            '<(DEPTH)/native_client/src/trusted/validator/ncfileutils.gyp:ncfileutils_x86_32',
            '<(DEPTH)/native_client/src/trusted/validator/x86/decoder/ncdis_decode_tables.gyp:ncdis_decode_tables_x86_32',
            '<(DEPTH)/native_client/src/trusted/validator/x86/decoder/ncval_x86_decoder.gyp:nc_opcode_modeling_verbose_x86_32',
            '<(DEPTH)/native_client/src/trusted/validator/x86/ncval_reg_sfi/ncval_reg_sfi.gyp:ncval_reg_sfi_verbose_x86_32',
            '<(DEPTH)/native_client/src/trusted/validator/x86/ncval_reg_sfi/ncval_reg_sfi.gyp:ncval_reg_sfi_x86_32',
            '<(DEPTH)/native_client/src/trusted/validator/x86/ncval_seg_sfi/ncval_seg_sfi.gyp:ncdis_seg_sfi_verbose_x86_32',
            '<(DEPTH)/native_client/src/trusted/validator/x86/ncval_seg_sfi/ncval_seg_sfi.gyp:ncval_seg_sfi_x86_32',
            '<(DEPTH)/native_client/src/trusted/validator/x86/validate_x86.gyp:ncval_base_verbose_x86_32',
            'ncdis_util_x86_32',
          ],
        }],
    }],
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'ncdis_util_x86_64',
          'type': 'static_library',
          'variables': {
            'target_base': 'ncdis_util',
            'win_target': 'x64',
          },
          'sources': ['ncenuminsts_x86_64.c'],
        }, {
          'target_name': 'ncval_x86_64',
          'type': 'executable',
          'variables': {
            'target_base': 'ncval',
            'win_target': 'x64',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/utils/utils.gyp:utils64',
            '<(DEPTH)/native_client/src/trusted/validator/ncfileutils.gyp:ncfileutils_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator/x86/decoder/ncdis_decode_tables.gyp:ncdis_decode_tables_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator/x86/decoder/ncval_x86_decoder.gyp:nc_opcode_modeling_verbose_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator/x86/ncval_reg_sfi/ncval_reg_sfi.gyp:ncval_reg_sfi_verbose_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator/x86/ncval_reg_sfi/ncval_reg_sfi.gyp:ncval_reg_sfi_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator/x86/ncval_seg_sfi/ncval_seg_sfi.gyp:ncdis_seg_sfi_verbose_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator/x86/ncval_seg_sfi/ncval_seg_sfi.gyp:ncval_seg_sfi_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator/x86/validate_x86.gyp:ncval_base_verbose_x86_64',
            'ncdis_util_x86_64',
          ],
        }],
    }],
    ['target_arch=="x64"', {
      'targets': [
        {
          'target_name': 'ncdis_util_x86_64',
          'type': 'static_library',
          'variables': {
            'target_base': 'ncdis_util',
          },
          'sources': ['ncenuminsts_x86_64.c'],
        }, {
          'target_name': 'ncval_x86_64',
          'type': 'executable',
          'variables': {
            'target_base': 'ncval',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/utils/utils.gyp:utils',
            '<(DEPTH)/native_client/src/trusted/validator/ncfileutils.gyp:ncfileutils_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator/x86/decoder/ncdis_decode_tables.gyp:ncdis_decode_tables_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator/x86/decoder/ncval_x86_decoder.gyp:nc_opcode_modeling_verbose_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator/x86/ncval_reg_sfi/ncval_reg_sfi.gyp:ncval_reg_sfi_verbose_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator/x86/ncval_reg_sfi/ncval_reg_sfi.gyp:ncval_reg_sfi_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator/x86/ncval_seg_sfi/ncval_seg_sfi.gyp:ncdis_seg_sfi_verbose_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator/x86/ncval_seg_sfi/ncval_seg_sfi.gyp:ncval_seg_sfi_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator/x86/validate_x86.gyp:ncval_base_verbose_x86_64',
            'ncdis_util_x86_64',
          ],
        }],
    }],
  ],
}
