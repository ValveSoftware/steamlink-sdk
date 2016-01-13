# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    'ipc.gypi',
  ],
  'targets': [
    {
      'target_name': 'ipc',
      'type': '<(component)',
      'variables': {
        'ipc_target': 1,
      },
      'dependencies': [
        '../base/base.gyp:base',
        # TODO(viettrungluu): Needed for base/lazy_instance.h, which is suspect.
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      ],
      # TODO(gregoryd): direct_dependent_settings should be shared with the
      # 64-bit target, but it doesn't work due to a bug in gyp
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
    },
    {
      'target_name': 'ipc_tests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'ipc',
        'test_support_ipc',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..'
      ],
      'sources': [
        'file_descriptor_set_posix_unittest.cc',
        'ipc_channel_posix_unittest.cc',
        'ipc_channel_proxy_unittest.cc',
        'ipc_channel_unittest.cc',
        'ipc_fuzzing_tests.cc',
        'ipc_message_unittest.cc',
        'ipc_message_utils_unittest.cc',
        'ipc_send_fds_test.cc',
        'ipc_sync_channel_unittest.cc',
        'ipc_sync_message_unittest.cc',
        'ipc_sync_message_unittest.h',
        'ipc_test_base.cc',
        'ipc_test_base.h',
        'run_all_unittests.cc',
        'sync_socket_unittest.cc',
        'unix_domain_socket_util_unittest.cc',
      ],
      'conditions': [
        ['OS == "win" or OS == "ios"', {
          'sources!': [
            'unix_domain_socket_util_unittest.cc',
          ],
        }],
        ['OS == "android"', {
          'dependencies': [
            '../testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
        ['os_posix == 1 and OS != "mac" and OS != "android"', {
          'conditions': [
            ['use_allocator!="none"', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }]
      ],
    },
    {
      'target_name': 'ipc_perftests',
      'type': '<(gtest_target_type)',
      # TODO(viettrungluu): Figure out which dependencies are really needed.
      'dependencies': [
        'ipc',
        'test_support_ipc',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../base/base.gyp:test_support_perf',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..'
      ],
      'sources': [
        'ipc_perftests.cc',
        'ipc_test_base.cc',
        'ipc_test_base.h',
      ],
      'conditions': [
        ['OS == "android"', {
          'dependencies': [
            '../testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
        ['os_posix == 1 and OS != "mac" and OS != "android"', {
          'conditions': [
            ['use_allocator!="none"', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }]
      ],
    },
    {
      'target_name': 'test_support_ipc',
      'type': 'static_library',
      'dependencies': [
        'ipc',
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'ipc_multiprocess_test.cc',
        'ipc_multiprocess_test.h',
        'ipc_test_sink.cc',
        'ipc_test_sink.h',
      ],
    },
  ],
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'ipc_win64',
          'type': '<(component)',
          'variables': {
            'ipc_target': 1,
          },
          'dependencies': [
            '../base/base.gyp:base_win64',
            # TODO(viettrungluu): Needed for base/lazy_instance.h, which is
            # suspect.
            '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations_win64',
          ],
          # TODO(gregoryd): direct_dependent_settings should be shared with the
          # 32-bit target, but it doesn't work due to a bug in gyp
          'direct_dependent_settings': {
            'include_dirs': [
              '..',
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
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'ipc_tests_apk',
          'type': 'none',
          'dependencies': [
            'ipc_tests',
          ],
          'variables': {
            'test_suite_name': 'ipc_tests',
          },
          'includes': [ '../build/apk_test.gypi' ],
        },
        {
          'target_name': 'ipc_perftests_apk',
          'type': 'none',
          'dependencies': [
            'ipc_perftests',
          ],
          'variables': {
            'test_suite_name': 'ipc_perftests',
          },
          'includes': [ '../build/apk_test.gypi' ],
        }],
    }],
  ],
}
