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
        'error_delegate_util.cc',
        'error_delegate_util.h',
        'init_status.h',
        'meta_table.cc',
        'meta_table.h',
        'recovery.cc',
        'recovery.h',
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
      ],
      'export_dependent_settings': [
        'sql',
        '../base/base.gyp:base',
      ],
      'sources': [
        'test/error_callback_support.cc',
        'test/error_callback_support.h',
        'test/scoped_error_ignorer.cc',
        'test/scoped_error_ignorer.h',
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
        '../base/base.gyp:run_all_unittests',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        '../third_party/sqlite/sqlite.gyp:sqlite',
      ],
      'sources': [
        'connection_unittest.cc',
        'meta_table_unittest.cc',
        'recovery_unittest.cc',
        'sqlite_features_unittest.cc',
        'statement_unittest.cc',
        'transaction_unittest.cc',
      ],
      'include_dirs': [
        '..',
      ],
      'conditions': [
        ['os_posix==1 and OS!="mac" and OS!="ios"', {
          'conditions': [
            ['use_allocator!="none"', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
        ['OS == "android"', {
          'dependencies': [
            '../testing/android/native_test.gyp:native_test_native_code',
          ],
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
          },
          'includes': [ '../build/apk_test.gypi' ],
        },
      ],
    }],
  ],
}
