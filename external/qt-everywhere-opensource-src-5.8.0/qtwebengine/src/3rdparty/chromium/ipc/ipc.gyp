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
        'ipc_interfaces',
        '../base/base.gyp:base',
        '../mojo/mojo_public.gyp:mojo_cpp_bindings',
        '../mojo/mojo_public.gyp:mojo_cpp_system',
      ],
      # TODO(gregoryd): direct_dependent_settings should be shared with the
      # 64-bit target, but it doesn't work due to a bug in gyp
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'conditions': [
        ['OS == "win" or OS == "mac"', {
          'dependencies': [
            '../crypto/crypto.gyp:crypto',
          ],
        }],
      ],
    },
    {
      'target_name': 'ipc_interfaces_mojom',
      'type': 'none',
      'variables': {
        'require_interface_bindings': 0,
        'mojom_files': [
          'ipc.mojom',
        ],
      },
      'includes': [ '../mojo/mojom_bindings_generator_explicit.gypi' ],
    },
    {
      'target_name': 'ipc_interfaces',
      'type': 'static_library',
      'dependencies': [
        'ipc_interfaces_mojom',
        '../base/base.gyp:base',
        '../mojo/mojo_public.gyp:mojo_interface_bindings_generation',
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
      'target_name': 'ipc_run_all_unittests',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../mojo/mojo_edk.gyp:mojo_common_test_support',
        '../mojo/mojo_edk.gyp:mojo_system_impl',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'run_all_unittests.cc',
      ],
    },
    {
      'target_name': 'ipc_tests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'ipc',
        'ipc_run_all_unittests',
        'test_support_ipc',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../crypto/crypto.gyp:crypto',
        '../mojo/mojo_public.gyp:mojo_cpp_bindings',
        '../mojo/mojo_public.gyp:mojo_cpp_system',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..'
      ],
      'sources': [
        'attachment_broker_mac_unittest.cc',
        'attachment_broker_privileged_mac_unittest.cc',
        'attachment_broker_privileged_win_unittest.cc',
        'ipc_channel_mojo_unittest.cc',
        'ipc_channel_posix_unittest.cc',
        'ipc_channel_proxy_unittest.cc',
        'ipc_channel_reader_unittest.cc',
        'ipc_channel_unittest.cc',
        'ipc_fuzzing_tests.cc',
        'ipc_message_attachment_set_posix_unittest.cc',
        'ipc_message_unittest.cc',
        'ipc_message_utils_unittest.cc',
        'ipc_mojo_bootstrap_unittest.cc',
        'ipc_send_fds_test.cc',
        'ipc_sync_channel_unittest.cc',
        'ipc_sync_message_unittest.cc',
        'ipc_sync_message_unittest.h',
        'ipc_test_messages.h',
        'ipc_test_message_generator.cc',
        'ipc_test_message_generator.h',
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
          'sources!': [
            # These multiprocess tests don't work on Android.
            'ipc_channel_unittest.cc',
          ],
          'dependencies': [
            '../testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
        ['OS == "mac"', {
          'dependencies': [
            '../sandbox/sandbox.gyp:seatbelt',
          ],
        }],
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
        '../mojo/mojo_edk.gyp:mojo_common_test_support',
        '../mojo/mojo_edk.gyp:mojo_system_impl',
        '../mojo/mojo_public.gyp:mojo_cpp_bindings',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..'
      ],
      'sources': [
        'ipc_mojo_perftest.cc',
        'ipc_perftests.cc',
        'ipc_test_base.cc',
        'ipc_test_base.h',
        'run_all_perftests.cc',
      ],
      'conditions': [
        ['OS == "android"', {
          'dependencies': [
            '../testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
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
        'ipc_perftest_support.cc',
        'ipc_perftest_support.h',
        'ipc_security_test_util.cc',
        'ipc_security_test_util.h',
        'ipc_test_base.cc',
        'ipc_test_base.h',
        'ipc_test_channel_listener.cc',
        'ipc_test_channel_listener.h',
        'ipc_test_sink.cc',
        'ipc_test_sink.h',
        'test_util_mac.cc',
        'test_util_mac.h',
      ],
    },
  ],
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'ipc_interfaces_win64',
          'type': 'static_library',
          'dependencies': [
            'ipc_interfaces_mojom',
            '../base/base.gyp:base_win64',
            '../mojo/mojo_public.gyp:mojo_interface_bindings_generation',
          ],
          'include_dirs': [
            '..',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
        {
          'target_name': 'ipc_win64',
          'type': '<(component)',
          'variables': {
            'ipc_target': 1,
          },
          'dependencies': [
            'ipc_interfaces_win64',
            '../base/base.gyp:base_win64',
            '../crypto/crypto.gyp:crypto_nacl_win64',
            '../mojo/mojo_public.gyp:mojo_cpp_bindings_win64',
            '../mojo/mojo_public.gyp:mojo_cpp_system_win64',
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
        }
      ],
      'conditions': [
        ['test_isolation_mode != "noop"', {
          'targets': [
            {
              'target_name': 'ipc_tests_apk_run',
              'type': 'none',
              'dependencies': [
                'ipc_tests_apk',
              ],
              'includes': [
                '../build/isolate.gypi',
              ],
              'sources': [
                'ipc_tests_apk.isolate',
              ],
            },
          ],
        }],
      ],
    }],
    ['test_isolation_mode != "noop" and OS != "android"', {
      'targets': [
        {
          'target_name': 'ipc_tests_run',
          'type': 'none',
          'dependencies': [
            'ipc_tests',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'ipc_tests.isolate',
          ],
        },
      ],
    }],
  ],
}
