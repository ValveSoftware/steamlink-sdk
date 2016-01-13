# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is a dummy target so as to not to break the build while moving tests from
# webkit_compositor_bindings_unittests to content_unittests.
{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'webkit_compositor_bindings_unittests',
      'type' : '<(gtest_target_type)',
      'dependencies': [
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/base/base.gyp:test_support_base',
      ],
      'sources': [
        'compositor_bindings_dummy_unittest.cc',
        'test/run_all_unittests.cc',
      ],
      'conditions': [
        ['OS == "android"', {
          'dependencies': [
            '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
        [ 'os_posix == 1 and OS != "mac" and OS != "android" and OS != "ios"', {
          'conditions': [
            [ 'use_allocator!="none"', {
              'dependencies': [
                '<(DEPTH)/base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'webkit_compositor_bindings_unittests_apk',
          'type': 'none',
          'dependencies': [
            'webkit_compositor_bindings_unittests',
          ],
          'variables': {
            'test_suite_name': 'webkit_compositor_bindings_unittests',
          },
          'includes': [ '../../../build/apk_test.gypi' ],
        },
      ],
    }],
  ],
}