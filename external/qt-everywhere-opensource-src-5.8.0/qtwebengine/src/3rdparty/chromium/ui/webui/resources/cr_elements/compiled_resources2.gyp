# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'cr_elements_resources',
      'type': 'none',
      'dependencies': [
        'cr_dialog/compiled_resources2.gyp:*',
        'cr_profile_avatar_selector/compiled_resources2.gyp:*',
        'cr_slider/compiled_resources2.gyp:*',
        'network/compiled_resources2.gyp:*',
        'policy/compiled_resources2.gyp:*',
      ],
    },
  ]
}
