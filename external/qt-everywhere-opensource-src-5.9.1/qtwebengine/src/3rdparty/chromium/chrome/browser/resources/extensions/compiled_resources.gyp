# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'extensions',
      'variables': {
        'depends': [
          '../../../../third_party/jstemplate/compiled_resources.gyp:jstemplate',
          '../../../../ui/webui/resources/js/assert.js',
          '../../../../ui/webui/resources/js/assert.js',
          '../../../../ui/webui/resources/js/cr.js',
          '../../../../ui/webui/resources/js/cr/event_target.js',
          '../../../../ui/webui/resources/js/cr/ui.js',
          '../../../../ui/webui/resources/js/cr/ui/alert_overlay.js',
          '../../../../ui/webui/resources/js/cr/ui/array_data_model.js',
          '../../../../ui/webui/resources/js/cr/ui/bubble.js',
          '../../../../ui/webui/resources/js/cr/ui/bubble_button.js',
          '../../../../ui/webui/resources/js/cr/ui/controlled_indicator.js',
          '../../../../ui/webui/resources/js/cr/ui/drag_wrapper.js',
          '../../../../ui/webui/resources/js/cr/ui/focus_manager.js',
          '../../../../ui/webui/resources/js/cr/ui/focus_outline_manager.js',
          '../../../../ui/webui/resources/js/cr/ui/list.js',
          '../../../../ui/webui/resources/js/cr/ui/list_item.js',
          '../../../../ui/webui/resources/js/cr/ui/list_selection_controller.js',
          '../../../../ui/webui/resources/js/cr/ui/list_selection_model.js',
          '../../../../ui/webui/resources/js/cr/ui/overlay.js',
          '../../../../ui/webui/resources/js/event_tracker.js',
          '../../../../ui/webui/resources/js/load_time_data.js',
          '../../../../ui/webui/resources/js/promise_resolver.js',
          '../../../../ui/webui/resources/js/util.js',
          'focus_row.js',
        ],
        'externs': [
          '<(EXTERNS_DIR)/chrome_send.js',
          '<(EXTERNS_DIR)/developer_private.js',
          '<(EXTERNS_DIR)/management.js',
        ],
      },
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    }
  ],
}
