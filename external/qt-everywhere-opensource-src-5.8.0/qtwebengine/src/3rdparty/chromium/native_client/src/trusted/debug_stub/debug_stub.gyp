#
# Copyright 2010 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
{
  'includes': [
    '../../../build/common.gypi',
  ],
  'variables': {
    'common_sources': [
      'abi.cc',
      'debug_stub.cc',
      'nacl_debug.cc',
      'packet.cc',
      'session.cc',
      'target.cc',
      'thread_common.cc',
      'transport_common.cc',
      "transport_ipc.cc",
      'util.cc',
    ],
    'test_sources': [
      'abi_test.cc',
      'packet_test.cc',
      'session_test.cc',
      'util_test.cc',
      'test.cc',
    ],
    'conditions': [
      ['os_posix==1', {
        'platform_sources': [
          'posix/debug_stub_posix.cc',
          'posix/platform_impl.cc',
          'posix/thread_impl.cc',
        ],
      }],
      ['OS=="win"', {
        'platform_sources': [
          'win/debug_stub_win.cc',
          'win/platform_impl.cc',
          'win/thread_impl.cc',
        ],
      }],
    ],
  },

  'target_defaults': {
    'variables': {
      'target_base': 'none',
    },
    'target_conditions': [
      ['<(os_posix)==1', {
        'cflags': [
          '-fexceptions',
        ],
        'cflags_cc' : [
          '-frtti',
        ]
      }],
      ['OS=="mac"', {
        'xcode_settings': {
          'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',      # -fexceptions
          'GCC_ENABLE_CPP_RTTI': 'YES',            # -frtti
        }
      }],
      ['<(os_posix)==1', {
        'cflags': [
          '-Wno-long-long',
        ],
      }],
      ['target_base=="debug_stub"', {
        'sources': [
          '<@(common_sources)',
          '<@(platform_sources)',
        ],
      }],
    ],
  },
  'targets': [
    # ----------------------------------------------------------------------
    {
      'target_name': 'debug_stub',
      'type': 'static_library',
      'variables': {
        'target_base': 'debug_stub',
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
      ],
    },
    {
      'target_name': 'gdb_rsp_test',
      'type': 'executable',
      'sources': [
        '<@(test_sources)',
      ],
      'dependencies': [
        'debug_stub',
      ]
    },
  ],
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        # ---------------------------------------------------------------------
        {
          'target_name': 'debug_stub64',
          'type': 'static_library',
          'variables': {
            'target_base': 'debug_stub',
            'win_target': 'x64',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
          ],
        },
        {
          'target_name': 'gdb_rsp_test64',
          'type': 'executable',
          'sources': [
            '<@(test_sources)',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
          'dependencies': [
            'debug_stub64',
          ]
        },
      ],
    }],
  ],
}

