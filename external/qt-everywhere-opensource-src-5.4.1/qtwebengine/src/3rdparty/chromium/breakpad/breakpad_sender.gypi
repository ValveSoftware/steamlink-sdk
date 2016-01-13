# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'breakpad_sender_target': 0,
    },
    'target_conditions': [
      ['breakpad_sender_target==1', {
        'sources': [
          'src/client/windows/sender/crash_report_sender.cc',
          'src/common/windows/http_upload.cc',
          'src/client/windows/sender/crash_report_sender.h',
          'src/common/windows/http_upload.h',
        ],
        'include_dirs': [
          'src',
        ],
      }],
    ],
  },
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'breakpad_sender',
          'type': 'static_library',
          'variables': {
            'breakpad_sender_target': 1,
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
          'target_name': 'breakpad_sender_win64',
          'type': 'static_library',
          'variables': {
            'breakpad_sender_target': 1,
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
