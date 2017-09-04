# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# NOTE: Created with generate_compiled_resources_gyp.py, please do not edit.
{
  'targets': [
    {
      'target_name': 'app-layout-extracted',
      'dependencies': [
        'app-box/compiled_resources2.gyp:app-box-extracted',
        'app-drawer-layout/compiled_resources2.gyp:app-drawer-layout-extracted',
        'app-drawer/compiled_resources2.gyp:app-drawer-extracted',
        'app-header-layout/compiled_resources2.gyp:app-header-layout-extracted',
        'app-header/compiled_resources2.gyp:app-header-extracted',
        'app-scrollpos-control/compiled_resources2.gyp:app-scrollpos-control-extracted',
        'app-toolbar/compiled_resources2.gyp:app-toolbar-extracted',
        'helpers/compiled_resources2.gyp:helpers-extracted',
      ],
      'includes': ['../../../../closure_compiler/compile_js2.gypi'],
    },
  ],
}
