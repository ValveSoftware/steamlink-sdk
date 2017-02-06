# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../build/common.gypi',
  ],
  'conditions': [
    ['target_arch=="ia32" or target_arch=="x64"', {
      'targets': [
        {
          'target_name': 'ncval_new',
          'type': 'executable',
          'sources': ['ncval.cc', 'elf_load.cc'],
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
            '<(DEPTH)/native_client/src/trusted/validator_arm/validator_arm.gyp:arm_validator_core',
            '<(DEPTH)/native_client/src/trusted/validator_arm/validator_arm.gyp:arm_validator_reporters',
            '<(DEPTH)/native_client/src/trusted/validator_ragel/rdfa_validator.gyp:rdfa_validator',
          ],
        },
      ]
    }],
  ],
}
