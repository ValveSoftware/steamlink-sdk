# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'cr_elements_resources',
      'type': 'none',
      'dependencies': [
        'cr_action_menu/compiled_resources2.gyp:*',
        'cr_dialog/compiled_resources2.gyp:*',
        'cr_profile_avatar_selector/compiled_resources2.gyp:*',
        'cr_slider/compiled_resources2.gyp:*',
        'network/compiled_resources2.gyp:*',
        'policy/compiled_resources2.gyp:*',
      ],
    },
    {
      'target_name': 'cr_scrollable_behavior',
      'dependencies': [
        '<(DEPTH)/third_party/polymer/v1_0/components-chromium/iron-list/compiled_resources2.gyp:iron-list-extracted',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
  ]
}
