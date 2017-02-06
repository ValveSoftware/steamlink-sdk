# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'main',
      'variables': {
        'depends': [
          '../../../../../third_party/jstemplate/compiled_resources.gyp:jstemplate',
          '../../../../../ui/webui/resources/js/cr.js',
          '../../../../../ui/webui/resources/js/cr/event_target.js',
          '../../../../../ui/webui/resources/js/cr/link_controller.js',
          '../../../../../ui/webui/resources/js/cr/ui.js',
          '../../../../../ui/webui/resources/js/cr/ui/array_data_model.js',
          '../../../../../ui/webui/resources/js/cr/ui/command.js',
          '../../../../../ui/webui/resources/js/cr/ui/context_menu_button.js',
          '../../../../../ui/webui/resources/js/cr/ui/context_menu_handler.js',
          '../../../../../ui/webui/resources/js/cr/ui/focus_outline_manager.js',
          '../../../../../ui/webui/resources/js/cr/ui/list.js',
          '../../../../../ui/webui/resources/js/cr/ui/list_item.js',
          '../../../../../ui/webui/resources/js/cr/ui/list_selection_controller.js',
          '../../../../../ui/webui/resources/js/cr/ui/list_selection_model.js',
          '../../../../../ui/webui/resources/js/cr/ui/menu.js',
          '../../../../../ui/webui/resources/js/cr/ui/menu_button.js',
          '../../../../../ui/webui/resources/js/cr/ui/menu_item.js',
          '../../../../../ui/webui/resources/js/cr/ui/position_util.js',
          '../../../../../ui/webui/resources/js/cr/ui/splitter.js',
          '../../../../../ui/webui/resources/js/cr/ui/touch_handler.js',
          '../../../../../ui/webui/resources/js/cr/ui/tree.js',
          '../../../../../ui/webui/resources/js/event_tracker.js',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:i18n_template_no_process',
          '../../../../../ui/webui/resources/js/load_time_data.js',
          '../../../../../ui/webui/resources/js/promise_resolver.js',
          '../../../../../ui/webui/resources/js/util.js',
          '../../../../../ui/webui/resources/js/icon.js',
          '../../../../../chrome/browser/resources/bookmark_manager/js/bmm.js',
          '../../../../../chrome/browser/resources/bookmark_manager/js/bmm/bookmark_list.js',
          '../../../../../chrome/browser/resources/bookmark_manager/js/bmm/bookmark_tree.js',
          '../../../../../chrome/browser/resources/bookmark_manager/js/dnd.js',
        ],
        'externs': [
          '<(EXTERNS_DIR)/bookmark_manager_private.js',
          '<(EXTERNS_DIR)/chrome_send.js',
          '<(EXTERNS_DIR)/chrome_extensions.js',
          '<(EXTERNS_DIR)/metrics_private.js',
          '<(EXTERNS_DIR)/system_private.js',
          '../../../../../ui/webui/resources/js/template_data_externs.js',
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    }
  ],
}
