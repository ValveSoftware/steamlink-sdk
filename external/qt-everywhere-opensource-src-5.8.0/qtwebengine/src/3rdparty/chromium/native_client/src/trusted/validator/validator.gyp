# -*- gyp -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'validators',
      'type': 'static_library',
      'sources' : [
        'validator_init.c',
      ],
    },
    {
      'target_name': 'validation_cache',
      'type': 'static_library',
      'sources' : [
        'validation_cache.c',
      ],
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
      ],
    },
  ],
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'validators64',
          'type': 'static_library',
          'sources' : [
            'validator_init.c',
          ],
          'variables': {
            'win_target': 'x64',
          },
        },
        {
          'target_name': 'validation_cache64',
          'type': 'static_library',
          'sources' : [
            'validation_cache.c',
          ],
          'variables': {
            'win_target': 'x64',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
          ],
        },
      ],
    }],
  ],
}
