# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ios/testing:ocmock_support
      'target_name': 'ocmock_support',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../third_party/ocmock/ocmock.gyp:ocmock',
        '../../third_party/google_toolbox_for_mac/google_toolbox_for_mac.gyp:google_toolbox_for_mac',
      ],
      'sources': [
        'ocmock_complex_type_helper.h',
        'ocmock_complex_type_helper.mm',
      ],
      'include_dirs': [
        '../..',
      ],
      'export_dependent_settings': [
        '../../third_party/google_toolbox_for_mac/google_toolbox_for_mac.gyp:google_toolbox_for_mac',
      ],
    },
    {
      # GN version: //ios/testing:ocmock_support_unittest
      'target_name': 'ocmock_support_unittest',
      'type': 'executable',
      'variables': {
        'ios_product_name': '<(_target_name)',
      },
      'sources': [
        'ocmock_complex_type_helper_unittest.mm',
      ],
      'dependencies': [
        'ocmock_support',
        '../../base/base.gyp:run_all_unittests',
        '../../base/base.gyp:test_support_base',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        '../../testing/iossim/iossim.gyp:iossim#host',
        '../../third_party/ocmock/ocmock.gyp:ocmock',
      ],
      'include_dirs': [
        '../..',
      ],
      'xcode_settings': {'OTHER_LDFLAGS': ['-ObjC']},
    },
  ],
}
