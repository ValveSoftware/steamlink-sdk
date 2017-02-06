# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'cr_shared_menu',
      'dependencies': [
        '../../js/compiled_resources2.gyp:assert',
        '../../js/compiled_resources2.gyp:cr',
        '../../js/compiled_resources2.gyp:util',
        '../../js/cr/ui/compiled_resources2.gyp:position_util',
        '<(EXTERNS_GYP):web_animations',
      ],

      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
  ],
}
