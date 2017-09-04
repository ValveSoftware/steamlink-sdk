# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# NOTE: Created with generate_compiled_resources_gyp.py, please do not edit.
{
  'targets': [
    {
      'target_name': 'app-drawer-layout-extracted',
      'dependencies': [
        '../../iron-media-query/compiled_resources2.gyp:iron-media-query-extracted',
        '../../iron-resizable-behavior/compiled_resources2.gyp:iron-resizable-behavior-extracted',
        '../app-drawer/compiled_resources2.gyp:app-drawer-extracted',
      ],
      'includes': ['../../../../../closure_compiler/compile_js2.gypi'],
    },
  ],
}
