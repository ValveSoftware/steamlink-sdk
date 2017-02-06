# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'settings_main_rendered',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'settings_main',
      'dependencies': [
        'settings_main_rendered',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:promise_resolver',
        '../settings_page/compiled_resources2.gyp:main_page_behavior',
        '../settings_page/compiled_resources2.gyp:settings_router',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
  ],
}
