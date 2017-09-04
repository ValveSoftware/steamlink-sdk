# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'settings_ui',
      'dependencies': [
        '<(DEPTH)/third_party/polymer/v1_0/components-chromium/app-layout/app-drawer/compiled_resources2.gyp:app-drawer-extracted',
        '<(DEPTH)/ui/webui/resources/cr_elements/cr_toolbar/compiled_resources2.gyp:cr_toolbar',
        '<(DEPTH)/ui/webui/resources/cr_elements/cr_toolbar/compiled_resources2.gyp:cr_toolbar_search_field',
        '../compiled_resources2.gyp:direction_delegate',
        '../compiled_resources2.gyp:global_scroll_target_behavior',
        '../prefs/compiled_resources2.gyp:prefs',
        '../settings_main/compiled_resources2.gyp:settings_main',
        'settings_ui_types',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'settings_ui_types',
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
  ],
}
