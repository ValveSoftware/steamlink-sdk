# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'jstemplate',
      'variables': {
        'depends': [
          'util.js',
          'jsevalcontext.js',
        ],
      },
      'includes': ['../../third_party/closure_compiler/compile_js.gypi'],
    }
  ],
}
