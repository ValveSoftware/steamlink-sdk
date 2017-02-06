# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'apps_page',
      'variables': {
        'depends': [
          '../../../../third_party/jstemplate/compiled_resources.gyp:jstemplate',
          '../../../../ui/webui/resources/js/action_link.js',
          '../../../../ui/webui/resources/js/cr.js',
          '../../../../ui/webui/resources/js/event_tracker.js',
          '../../../../ui/webui/resources/js/load_time_data.js',
          '../../../../ui/webui/resources/js/parse_html_subset.js',
          '../../../../ui/webui/resources/js/promise_resolver.js',
          '../../../../ui/webui/resources/js/util.js',
          '../../../../ui/webui/resources/js/cr/event_target.js',
          '../../../../ui/webui/resources/js/cr/ui.js',
          '../../../../ui/webui/resources/js/cr/ui/bubble.js',
          '../../../../ui/webui/resources/js/cr/ui/card_slider.js',
          '../../../../ui/webui/resources/js/cr/ui/command.js',
          '../../../../ui/webui/resources/js/cr/ui/context_menu_handler.js',
          '../../../../ui/webui/resources/js/cr/ui/drag_wrapper.js',
          '../../../../ui/webui/resources/js/cr/ui/expandable_bubble.js',
          '../../../../ui/webui/resources/js/cr/ui/focus_manager.js',
          '../../../../ui/webui/resources/js/cr/ui/menu.js',
          '../../../../ui/webui/resources/js/cr/ui/menu_item.js',
          '../../../../ui/webui/resources/js/cr/ui/position_util.js',
          '../../../../ui/webui/resources/js/cr/ui/menu_button.js',
          '../../../../ui/webui/resources/js/cr/ui/context_menu_button.js',
          '../../../../ui/webui/resources/js/cr/ui/touch_handler.js',
          'logging.js',
          'tile_page.js',
          'dot_list.js',
          'trash.js',
          'page_switcher.js',
          'page_list_view.js',
          'nav_dot.js',
          'new_tab.js',
        ],
        'externs': ['<(EXTERNS_DIR)/chrome_send.js'],
      },
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    }
  ],
}
