# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'options_bundle',
      'variables': {
        'depends': [
          '../../../../third_party/jstemplate/compiled_resources.gyp:jstemplate',
          '../../../../ui/webui/resources/cr_elements/network/cr_onc_types.js',
          '../../../../ui/webui/resources/js/action_link.js',
          '../../../../ui/webui/resources/js/cr.js',
          '../../../../ui/webui/resources/js/cr/event_target.js',
          '../../../../ui/webui/resources/js/cr/ui.js',
          '../../../../ui/webui/resources/js/cr/ui/array_data_model.js',
          '../../../../ui/webui/resources/js/cr/ui/autocomplete_list.js',
          '../../../../ui/webui/resources/js/cr/ui/bubble.js',
          '../../../../ui/webui/resources/js/cr/ui/bubble_button.js',
          '../../../../ui/webui/resources/js/cr/ui/command.js',
          '../../../../ui/webui/resources/js/cr/ui/controlled_indicator.js',
          '../../../../ui/webui/resources/js/cr/ui/focus_manager.js',
          '../../../../ui/webui/resources/js/cr/ui/focus_outline_manager.js',
          '../../../../ui/webui/resources/js/cr/ui/grid.js',
          '../../../../ui/webui/resources/js/cr/ui/list.js',
          '../../../../ui/webui/resources/js/cr/ui/list_item.js',
          '../../../../ui/webui/resources/js/cr/ui/list_selection_controller.js',
          '../../../../ui/webui/resources/js/cr/ui/list_selection_model.js',
          '../../../../ui/webui/resources/js/cr/ui/list_single_selection_model.js',
          '../../../../ui/webui/resources/js/cr/ui/menu.js',
          '../../../../ui/webui/resources/js/cr/ui/menu_item.js',
          '../../../../ui/webui/resources/js/cr/ui/overlay.js',
          '../../../../ui/webui/resources/js/cr/ui/position_util.js',
          '../../../../ui/webui/resources/js/cr/ui/page_manager/page.js',
          '../../../../ui/webui/resources/js/cr/ui/page_manager/page_manager.js',
          '../../../../ui/webui/resources/js/cr/ui/repeating_button.js',
          '../../../../ui/webui/resources/js/cr/ui/touch_handler.js',
          '../../../../ui/webui/resources/js/cr/ui/tree.js',
          '../../../../ui/webui/resources/js/event_tracker.js',
          '../../../../ui/webui/resources/js/icon.js',
          '../../../../ui/webui/resources/js/load_time_data.js',
          '../../../../ui/webui/resources/js/parse_html_subset.js',
          '../../../../ui/webui/resources/js/promise_resolver.js',
          '../../../../ui/webui/resources/js/util.js',
          '../../../../chrome/browser/resources/chromeos/keyboard/keyboard_utils.js',
        ],
        # options_bundle is included as a complex dependency. Currently there is
        # no possibility to use gyp variable expansion to it, so we don't use
        # <(CLOSURE_DIR) in the "externs" line.
        'externs': [
          '../../../../third_party/closure_compiler/externs/bluetooth.js',
          '../../../../third_party/closure_compiler/externs/bluetooth_private.js',
          '../../../../third_party/closure_compiler/externs/management.js',
          '../../../../third_party/closure_compiler/externs/metrics_private.js',
          '../../../../third_party/closure_compiler/externs/networking_private.js',
          '../../../../third_party/closure_compiler/externs/chrome_send.js',
          '../../../../ui/webui/resources/cr_elements/network/cr_network_icon_externs.js',
	],
      },
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    }
  ],
}
