# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'pepper_hash_for_uma',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base'
      ],
      'sources': [
        'pepper_hash_for_uma.cc',
      ],
    }
  ]
}
