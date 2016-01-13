# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
   },
  'targets': [
    {
      'target_name': 'ios_provider_web',
      'type': 'none',
      'include_dirs': [
        '..',
      ],
      'sources': [
        'public/provider/web/web_state.h',
        'public/provider/web/web_state_user_data.h',
      ],
      'dependencies': [
        'ios_base.gyp:ios_consumer_base',
      ],
    },
  ],
}
