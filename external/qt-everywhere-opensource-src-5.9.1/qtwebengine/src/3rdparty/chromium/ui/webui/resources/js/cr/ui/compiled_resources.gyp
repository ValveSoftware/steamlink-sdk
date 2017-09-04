# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'focus_manager',
      'variables': {
        'depends': [
          '../../assert.js',
          '../../compiled_resources.gyp:cr',
        ],
      },
      'includes': ['../../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'overlay',
      'variables': {
        'depends': [
          '../../cr.js',
          '../../promise_resolver.js',
          '../../util.js',
        ],
        'externs': [
          '../../../../../../third_party/closure_compiler/externs/chrome_send.js',
        ],
      },
      'includes': ['../../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
  ],
}
