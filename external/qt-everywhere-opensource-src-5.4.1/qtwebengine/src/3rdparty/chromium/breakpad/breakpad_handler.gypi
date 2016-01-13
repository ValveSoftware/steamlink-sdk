# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'breakpad_handler_target': 0,
    },
    'target_conditions': [
      ['breakpad_handler_target==1', {
        'sources': [
          'src/client/windows/crash_generation/client_info.cc',
          'src/client/windows/crash_generation/client_info.h',
          'src/client/windows/crash_generation/crash_generation_client.cc',
          'src/client/windows/crash_generation/crash_generation_client.h',
          'src/client/windows/crash_generation/crash_generation_server.cc',
          'src/client/windows/crash_generation/crash_generation_server.h',
          'src/client/windows/handler/exception_handler.cc',
          'src/client/windows/handler/exception_handler.h',
          'src/common/windows/guid_string.cc',
          'src/common/windows/guid_string.h',
          'src/google_breakpad/common/minidump_format.h',
          'src/client/windows/crash_generation/minidump_generator.cc',
          'src/client/windows/crash_generation/minidump_generator.h',
          'src/common/windows/string_utils-inl.h',
        ],
        'include_dirs': [
          'src',
        ],
        'defines': [
          # Avoid the TerminateThread Application Verifier Failure.
          'BREAKPAD_NO_TERMINATE_THREAD',
        ],
      }],
    ],
  },
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'breakpad_handler',
          'type': 'static_library',
          'variables': {
            'breakpad_handler_target': 1,
          },
          # TODO(gregoryd): direct_dependent_settings should be shared with the
          # 64-bit target, but it doesn't work due to a bug in gyp
          'direct_dependent_settings': {
            'include_dirs': [
              'src',
            ],
          },
        },
      ],
    }],
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'breakpad_handler_win64',
          'type': 'static_library',
          'variables': {
            'breakpad_handler_target': 1,
          },
          # TODO(gregoryd): direct_dependent_settings should be shared with the
          # 32-bit target, but it doesn't work due to a bug in gyp
          'direct_dependent_settings': {
            'include_dirs': [
              'src',
            ],
          },
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
  ],
}
