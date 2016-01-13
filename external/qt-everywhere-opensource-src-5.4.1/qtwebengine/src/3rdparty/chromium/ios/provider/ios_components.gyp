# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
   },
  'targets': [
    {
      'target_name': 'ios_components',
      'type': 'none',
      'include_dirs': [
        '../..',
      ],
      'sources': [
        '../public/provider/components/signin/browser/profile_oauth2_token_service_ios_provider.h',
      ] 
    },
  ],
}
