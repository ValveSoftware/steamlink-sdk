# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'main',
      'variables': {
        'depends': [
          '<@(cws_widget_container)',
          '../../../../third_party/jstemplate/compiled_resources.gyp:jstemplate',
          '../../../../ui/webui/resources/js/compiled_resources.gyp:i18n_template_no_process',
          '../../../../ui/webui/resources/js/load_time_data.js',
        ],
        'externs': [
          '<(EXTERNS_DIR)/chrome_send.js',
          '<(EXTERNS_DIR)/chrome_extensions.js',
          '<(EXTERNS_DIR)/file_manager_private.js',
          '<(EXTERNS_DIR)/metrics_private.js',
          '../externs/chrome_webstore_widget_private.js',
          '../externs/webview_tag.js'
        ]
      },
      'includes': [
        '../../../../third_party/closure_compiler/compile_js.gypi',
        '../cws_widget/compiled_resources.gypi'
      ]
    },
    {
      'target_name': 'background',
      'variables': {
        'externs': [
          '<(EXTERNS_DIR)/chrome_extensions.js',
          '../externs/chrome_webstore_widget_private.js'
        ]
      },
      'includes': [
        '../../../../third_party/closure_compiler/compile_js.gypi'
      ]
    }
  ]
}
