# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'advanced_page',
      'dependencies': [
        '../settings_page/compiled_resources2.gyp:main_page_behavior',
        '../settings_page/compiled_resources2.gyp:settings_page_visibility',
        '../settings_page/compiled_resources2.gyp:settings_router',
        '../settings_page/compiled_resources2.gyp:transition_behavior',
        '../system_page/compiled_resources2.gyp:system_page',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
  ],
}
