# Copyright 2012, Google Inc.
# Copyright 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  # ----------------------------------------------------------------------
  # Default settings
  # ----------------------------------------------------------------------

  'includes': [
    '../../../build/common.gypi',
  ],
  'variables': {
    'validate_gen_out':
      '<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_mips',
  },
  # TODO(robertm): move these setting to some global config
  'target_defaults': {
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
  'include_dirs': ['<(SHARED_INTERMEDIATE_DIR)'],
    # TODO(robertm): mmentovai does not think this is necessary
    # When ncvalidate is a dependency, it needs to be a hard dependency
    # because dependents may rely on ncvalidate to create header files below.
    'hard_dependency': 1,
    },
  # ----------------------------------------------------------------------
  # actual targets
  # ----------------------------------------------------------------------
    'targets': [
    # ----------------------------------------------------------------------
    {
      'target_name': 'mips_validator_core',
      'type': 'static_library',
      'sources': [
        'address_set.cc',
        'validator.cc',
        '<(validate_gen_out)/decode.cc'
      ],
      'dependencies': [
        '<(DEPTH)/native_client/src/trusted/cpu_features/cpu_features.gyp:cpu_features',
        'decode_gen'
      ],
    },
    # ----------------------------------------------------------------------
    {
      'target_name': 'decode_gen',
      'type': 'none',
      'direct_dependent_settings': {
        'include_dirs': ['<(SHARED_INTERMEDIATE_DIR)'],
      },
      'actions': [
       {
          'action_name': 'decode_gen_action',
          'msvs_cygwin_shell': 0,
          'inputs': [
            'mips-opt.table',
            'dgen/generate_decoder.py',
            'dgen/dgen_core.py',
            'dgen/dgen_input.py',
            'dgen/dgen_opt.py',
            'dgen/dgen_output.py',
          ],
          'outputs': [
            '<(validate_gen_out)/decode.cc',
          ],
          'action': [
            'python',
            'dgen/generate_decoder.py',
            'mips-opt.table',
            '<@(_outputs)',
           ],
          'process_outputs_as_sources': 1,
          'message': 'generate decoder.cc',
        },
      ],
    },
    # ----------------------------------------------------------------------
    {
      'target_name': 'ncvalidate_mips',
      'type': 'static_library',
      'sources': ['ncvalidate.cc'],
      'dependencies': [
        'mips_validator_core'
      ],
    },
  ],
}
