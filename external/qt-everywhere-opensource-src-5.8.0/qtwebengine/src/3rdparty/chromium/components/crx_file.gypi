# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'crx_file',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../crypto/crypto.gyp:crypto',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'crx_file/crx_file.cc',
        'crx_file/crx_file.h',
        'crx_file/id_util.cc',
        'crx_file/id_util.h',
      ],
    },
  ],
}
