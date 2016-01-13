# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'conditions': [
    ['target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'ncval_arm',
          'type': 'executable',
          'sources': [ 'ncval.cc' ],
          'dependencies': [
            '<(DEPTH)/native_client/src/trusted/validator/ncfileutils.gyp:ncfileutils_x86_32',
            'validator_arm.gyp:arm_validator_core',
            'validator_arm.gyp:arm_validator_reporters',
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
          ],
        }],
    }],
    ['target_arch=="x64"', {
      'targets': [
        {
          'target_name': 'ncval_arm',
          'type': 'executable',
          'sources': [ 'ncval.cc' ],
          'dependencies': [
            '<(DEPTH)/native_client/src/trusted/validator/ncfileutils.gyp:ncfileutils_x86_64',
            'validator_arm.gyp:arm_validator_core',
            'validator_arm.gyp:arm_validator_reporters',
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
          ],
        }],
    }],
  ],
}
