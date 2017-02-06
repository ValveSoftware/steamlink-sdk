# Copyright 2010, Google Inc.
# Copyright 2009 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  # ----------------------------------------------------------------------
  # Default settings
  # ----------------------------------------------------------------------

  'includes': [
    '../../../build/common.gypi',
  ],
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
      'target_name': 'arm_validator_core',
      'type': 'static_library',
      'sources': [
        'address_set.cc',
        'inst_classes.cc',
        'model.cc',
        'arm_helpers.cc',
        'validator.cc',
        'gen/arm32_decode.cc',
        'gen/arm32_decode_actuals_1.cc',
        'gen/arm32_decode_actuals_2.cc'
      ],
      'dependencies': [
        '<(DEPTH)/native_client/src/trusted/cpu_features/cpu_features.gyp:cpu_features'
      ],
    },
    # ----------------------------------------------------------------------
    {
      'target_name': 'ncvalidate_arm_v2',
      'type': 'static_library',
      'sources': [ 'ncvalidate.cc' ],
      'dependencies': [
        'arm_validator_core',
        '<(DEPTH)/native_client/src/trusted/validator/validator.gyp:validation_cache',
      ],
    },
    # ----------------------------------------------------------------------
    {
      'target_name': 'arm_validator_reporters',
      'type': 'static_library',
      'sources': [ 'problem_reporter.cc' ],
    },
  ],
}
