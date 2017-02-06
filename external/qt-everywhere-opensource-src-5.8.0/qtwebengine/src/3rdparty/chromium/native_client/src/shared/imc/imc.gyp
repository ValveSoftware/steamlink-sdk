# -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  ######################################################################
  'variables': {
    'COMMAND_TESTER': '<(DEPTH)/native_client/tools/command_tester.py',
    'common_sources': [
      'nacl_imc_common.cc',
      'nacl_imc_c.h',
    ],
    'conditions': [
      ['OS=="linux" or OS=="android"', {
        'common_sources': [
          'linux/nacl_imc.cc',
          'posix/nacl_imc_posix.cc',
        ],
      }],
      ['OS=="mac"', {
        'common_sources': [
          'osx/nacl_imc.cc',
          'posix/nacl_imc_posix.cc',
        ],
      }],
      ['OS=="win"', {
        'common_sources': [
          'win/nacl_imc.cc',
          'win/nacl_shm.cc',
        ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'ExceptionHandling': '2',  # /EHsc
          },
        },
      }],
    ],
  },
  ######################################################################
  'includes': [
    '../../../build/common.gypi',
  ],
  ######################################################################
  'target_defaults': {
  },
  'targets': [
    # ----------------------------------------------------------------------
    {
      'target_name': 'imc',
      'type': 'static_library',
      'sources': [
        '<@(common_sources)',
      ],
      'conditions': [
        ['OS=="linux" or OS=="android"', {
          'all_dependent_settings': {
            'libraries': [ '-lrt' ],
          }
        }],
      ],
    },
    # ----------------------------------------------------------------------
    # ----------------------------------------------------------------------
    {
      'target_name': 'sigpipe_test',
      'type': 'executable',
      'sources': [
        'sigpipe_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/imc/imc.gyp:imc',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio',
      ],
    },
    {
      'target_name': 'imc_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libimc.a',
        'nso_target': 'libimc.so',
        'build_glibc': 1,
        'build_newlib': 1,
        'build_pnacl_newlib': 1,
        'sources': ['nacl_imc_common.cc', 'nacl/nacl_imc.cc'],
      },
    },
  ],
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        # ---------------------------------------------------------------------
        {
          'target_name': 'imc64',
          'type': 'static_library',
          'variables': {
            'win_target': 'x64',
          },
          'sources': [
            '<@(common_sources)',
          ],
        },
        # ---------------------------------------------------------------------
        {
          'target_name': 'sigpipe_test64',
          'type': 'executable',
          'variables': {
            'win_target': 'x64',
          },
          'sources': [
            'sigpipe_test.cc',
          ],
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/imc/imc.gyp:imc64',
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
            '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio64',
          ],
        },
      ],
    }],
  ],
}

# TODO: some tests missing, c.f. build.scons

