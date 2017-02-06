# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/cookie_config
      'target_name': 'cookie_config',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        'os_crypt',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'cookie_config/cookie_store_util.cc',
        'cookie_config/cookie_store_util.h',
      ],
    },
  ],
}

