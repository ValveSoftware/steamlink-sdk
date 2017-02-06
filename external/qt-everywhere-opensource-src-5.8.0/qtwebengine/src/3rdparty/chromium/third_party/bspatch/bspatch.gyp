# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'bspatch',
      'type': 'static_library',
      'dependencies': [
        '../lzma_sdk/lzma_sdk.gyp:lzma_sdk',
      ],
      'include_dirs': [
        '.',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
      'sources': [
        'mbspatch.cc',
        'mbspatch.h',
      ],
    },
  ],
}
