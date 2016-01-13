# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Build the specific library dependencies for decoding x86 instructions.
# Used to define a "full" decoder (used by ncdis). Currently used to decode
# instructions for the x86-64 validator. Future plans is to encorporate the
# decoder into the x86-32 validator.
{
  'includes': [
    '../../../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="nc_opcode_modeling"', {
        'sources': ['ncopcode_desc.c'],
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
      ['target_base=="nc_opcode_modeling_verbose"', {
        'sources': ['ncopcode_desc_verbose.c'],
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
          ]
        },
      }],
      ['target_base=="nc_decoder"', {
        'sources': [
          'nc_inst_iter.c',
          'nc_inst_state.c',
          'nc_inst_trans.c',
          'ncop_exps.c',
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
          'target_name': 'nc_opcode_modeling_x86_32',
          'type': 'static_library',
          'hard_dependency': 1,
          'variables': {
            'target_base': 'nc_opcode_modeling',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/trusted/validator/x86/validate_x86.gyp:ncval_base_x86_32',
           ],
        }, {
          'target_name': 'nc_opcode_modeling_verbose_x86_32',
          'type': 'static_library',
          'hard_dependency': 1,
          'variables': {
            'target_base': 'nc_opcode_modeling_verbose',
          },
          'dependencies': [
            'nc_opcode_modeling_x86_32',
           ],
        }, {
          'target_name': 'nc_decoder_x86_32',
          'type': 'static_library',
          'variables': {
            'target_base': 'nc_decoder',
          },
          'dependencies': [
            'nc_opcode_modeling_x86_32',
            '<(DEPTH)/native_client/src/trusted/validator/x86/validate_x86.gyp:ncval_base_x86_32',
           ],
        }],
    }],
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'nc_opcode_modeling_x86_64',
          'type': 'static_library',
          'hard_dependency': 1,
          'variables': {
            'target_base': 'nc_opcode_modeling',
            'win_target': 'x64',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/trusted/validator/x86/validate_x86.gyp:ncval_base_x86_64',
           ],
        }, {
          'target_name': 'nc_opcode_modeling_verbose_x86_64',
          'type': 'static_library',
          'hard_dependency': 1,
          'variables': {
            'target_base': 'nc_opcode_modeling_verbose',
            'win_target': 'x64',
          },
          'dependencies': [
            'nc_opcode_modeling_x86_64',
           ],
        }, {
          'target_name': 'nc_decoder_x86_64',
          'type': 'static_library',
          'variables': {
            'target_base': 'nc_decoder',
            'win_target': 'x64',
          },
          'dependencies': [
            'nc_opcode_modeling_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator/x86/validate_x86.gyp:ncval_base_x86_64',
           ],
        }],
    }],
    ['target_arch=="x64"', {
      'targets': [
        {
          'target_name': 'nc_opcode_modeling_x86_64',
          'type': 'static_library',
          'hard_dependency': 1,
          'variables': {
            'target_base': 'nc_opcode_modeling',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/trusted/validator/x86/validate_x86.gyp:ncval_base_x86_64',
           ],
        }, {
          'target_name': 'nc_opcode_modeling_verbose_x86_64',
          'type': 'static_library',
          'hard_dependency': 1,
          'variables': {
            'target_base': 'nc_opcode_modeling_verbose',
          },
          'dependencies': [
            'nc_opcode_modeling_x86_64',
           ],
        }, {
          'target_name': 'nc_decoder_x86_64',
          'type': 'static_library',
          'variables': {
            'target_base': 'nc_decoder',
          },
          'dependencies': [
            'nc_opcode_modeling_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator/x86/validate_x86.gyp:ncval_base_x86_64',
           ],
        }],
    }],
  ],
}
