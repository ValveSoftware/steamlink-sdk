# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'metro_utils',
      'type': 'static_library',
      'dependencies': [
        'installer_util',
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'browser/metro_utils/metro_chrome_win.cc',
        'browser/metro_utils/metro_chrome_win.h',
      ],
    },
  ],
}
