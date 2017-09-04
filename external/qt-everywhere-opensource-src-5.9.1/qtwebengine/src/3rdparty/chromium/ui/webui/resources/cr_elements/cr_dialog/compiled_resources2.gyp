# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'cr_dialog',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
        '<(DEPTH)/third_party/polymer/v1_0/components-chromium/paper-dialog-behavior/compiled_resources2.gyp:paper-dialog-behavior-extracted',
        '<(DEPTH)/third_party/polymer/v1_0/components-chromium/paper-icon-button/compiled_resources2.gyp:paper-icon-button-extracted',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
  ],
}
