# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'libsync',
      'type': 'static_library',
      'sources': [
        'include/linux/sw_sync.h',
        'include/linux/sync.h',
        'include/sw_sync.h',
        'include/sync/sync.h',
        'strlcpy.c',
        'sync.c',
      ],
      'include_dirs': [
        'include',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
        ],
      },
    },
  ],
}
