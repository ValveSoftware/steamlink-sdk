# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'child',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_child',
        'chrome_features.gyp:chrome_common_features',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'child/pdf_child_init.cc',
        'child/pdf_child_init.h',
      ],
    },
  ],
}
