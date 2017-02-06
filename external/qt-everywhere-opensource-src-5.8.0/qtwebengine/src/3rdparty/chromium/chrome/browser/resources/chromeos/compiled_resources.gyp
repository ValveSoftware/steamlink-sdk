# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'bluetooth_pair_device',
      'variables': {
        'depends': [
          '../../../../chrome/browser/resources/options/compiled_resources.gyp:options_bundle',
        ],
        'externs': ['<(EXTERNS_DIR)/chrome_send.js'],
      },
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'certificate_manager_dialog',
      'variables': {
        'depends': [
          '../../../../chrome/browser/resources/options/compiled_resources.gyp:options_bundle',
        ],
        'externs': ['<(EXTERNS_DIR)/chrome_send.js'],
      },
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'quick_unlock/pin_keyboard',
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    }
  ],
}
