# Copyright 2013 The Native Client Authors. All rights reserved.
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
      'target_name': 'cpu_features',
      'type': 'static_library',
      # TODO(jfb) See TODO in build.scons on why x86 is built this way.
      'conditions': [
        ['target_arch=="ia32" or target_arch=="x64"', {
          'sources': [
            'arch/arm/cpu_arm.c',
            'arch/mips/cpu_mips.c',
            'arch/x86/cpu_x86.c',
            'arch/x86/cpu_xgetbv.S',
          ],
        }],
        ['target_arch=="arm" or target_arch=="mipsel"', {
          'sources': [
            'arch/arm/cpu_arm.c',
            'arch/mips/cpu_mips.c',
          ],
        }],
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
          'target_name': 'cpu_features64',
          'type': 'static_library',
          'variables': {
            'win_target': 'x64',
          },
          'sources': [
            'arch/arm/cpu_arm.c',
            'arch/mips/cpu_mips.c',
            'arch/x86/cpu_x86.c',
            'arch/x86/cpu_xgetbv.S',
          ],
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
          ],
        },
      ],
    },
   ],
  ],
}
