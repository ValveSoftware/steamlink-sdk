# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Build the specific library dependencies for validating on x86-32 using the
# DFA-based validator.
{
  'includes': [
    '../../../build/common.gypi',
  ],
  'conditions': [
    ['target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'dfa_validate_x86_32',
          'type': 'static_library',
          'sources' : [
            'dfa_validate_32.c',
            'dfa_validate_common.c',
            'validator_features_validator.c',
            'gen/validator_x86_32.c',
          ],
        },
      ],
    }],
  ],
}

