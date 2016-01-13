# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'plugin',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_plugin',
        'chrome_resources.gyp:chrome_strings',
      ],
      'include_dirs': [
        '..',
        '<(grit_out_dir)',
      ],
      'sources': [
        'plugin/chrome_content_plugin_client.cc',
        'plugin/chrome_content_plugin_client.h',
      ],
    },
  ],
}
