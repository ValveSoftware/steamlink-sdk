# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
   },
  'targets': [
    {
      'target_name': 'test_support_ios',
      'type': 'static_library',
      'sources': [
        'public/test/fake_profile_oauth2_token_service_ios_provider.h',
        'public/test/fake_profile_oauth2_token_service_ios_provider.mm',
      ],
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
    },
  ],
}
