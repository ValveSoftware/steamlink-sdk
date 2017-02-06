# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'dll_hash',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'dll_hash/dll_hash.cc',
        'dll_hash/dll_hash.h',
      ],
    },
    {
      'target_name': 'dll_hash_main',
      'type': 'executable',
      'dependencies': [
        'dll_hash',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'dll_hash/dll_hash_main.cc',
      ],
    }
  ]
}