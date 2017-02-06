# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'sql',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/sqlite/sqlite.gyp:sqlite',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
      ],
      'defines': [ 'SQL_IMPLEMENTATION' ],
      'sources': [
        'connection.cc',
        'connection.h',
        'connection_memory_dump_provider.cc',
        'connection_memory_dump_provider.h',
        'error_delegate_util.cc',
        'error_delegate_util.h',
        'init_status.h',
        'meta_table.cc',
        'meta_table.h',
        'recovery.cc',
        'recovery.h',
        'sql_memory_dump_provider.cc',
        'sql_memory_dump_provider.h',
        'statement.cc',
        'statement.h',
        'transaction.cc',
        'transaction.h',
      ],
      'include_dirs': [
        '..',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
    {
      'target_name': 'test_support_sql',
      'type': 'static_library',
      'dependencies': [
        'sql',
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        '../third_party/sqlite/sqlite.gyp:sqlite',
      ],
      'export_dependent_settings': [
        'sql',
        '../base/base.gyp:base',
      ],
      'sources': [
        'test/error_callback_support.cc',
        'test/error_callback_support.h',
        'test/scoped_error_expecter.cc',
        'test/scoped_error_expecter.h',
        'test/test_helpers.cc',
        'test/test_helpers.h',
      ],
      'include_dirs': [
        '..',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
    },
    {
      'target_name': 'sql_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'sql',
        'test_support_sql',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        '../third_party/sqlite/sqlite.gyp:sqlite',
      ],
      'sources': [
        'connection_unittest.cc',
        'meta_table_unittest.cc',
        'recovery_unittest.cc',
        'sql_memory_dump_provider_unittest.cc',
        'sqlite_features_unittest.cc',
        'statement_unittest.cc',
        'test/paths.cc',
        'test/paths.h',
        'test/run_all_unittests.cc',
        'test/sql_test_base.cc',
        'test/sql_test_base.h',
        'test/sql_test_suite.cc',
        'test/sql_test_suite.h',
        'transaction_unittest.cc',
      ],
      'include_dirs': [
        '..',
      ],
      'conditions': [
        ['OS == "android"', {
          'dependencies': [
            '../testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
        ['OS == "ios"', {
          'actions': [{
            'action_name': 'copy_test_data',
            'variables': {
              'test_data_files': [
                'test/data',
              ],
              'test_data_prefix' : 'sql',
            },
            'includes': [ '../build/copy_test_data_ios.gypi' ],
          }],
        }],
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'sql_unittests_apk',
          'type': 'none',
          'dependencies': [
            'sql_unittests',
          ],
          'variables': {
            'test_suite_name': 'sql_unittests',
            'isolate_file': 'sql_unittests.isolate',
          },
          'includes': [ '../build/apk_test.gypi' ],
        },
      ],
      'conditions': [
        ['test_isolation_mode != "noop"', {
          'targets': [
            {
              'target_name': 'sql_unittests_apk_run',
              'type': 'none',
              'dependencies': [
                'sql_unittests_apk',
              ],
              'includes': [
                '../build/isolate.gypi',
              ],
              'sources': [
                'sql_unittests_apk.isolate',
              ],
            },
          ]
        }],
      ]
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'sql_unittests_run',
          'type': 'none',
          'dependencies': [
            'sql_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'sql_unittests.isolate',
          ],
        },
      ],
    }],
  ],
}
