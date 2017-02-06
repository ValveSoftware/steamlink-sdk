# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ui_chromeos_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ui/chromeos/resources',
      },
      'actions': [
        {
          'action_name': 'ui_chromeos_resources',
          'variables': {
            'grit_grd_file': 'resources/ui_chromeos_resources.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../build/grit_target.gypi' ],
    },  # target_name: ui_chromeos_resources
    {
      'target_name': 'ui_chromeos_strings',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ui/chromeos/strings',
      },
      'actions': [
        {
          'action_name': 'generate_ui_chromeos_strings',
          'variables': {
            'grit_grd_file': 'ui_chromeos_strings.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../build/grit_target.gypi' ],
    },  # target_name: ui_chromeos_strings
    {
      'target_name': 'ui_chromeos',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../chromeos/chromeos.gyp:chromeos',
        '../../chromeos/chromeos.gyp:power_manager_proto',
        '../../components/components.gyp:device_event_log_component',
        '../../components/components.gyp:onc_component',
        '../../skia/skia.gyp:skia',
        '../aura/aura.gyp:aura',
        '../base/ime/ui_base_ime.gyp:ui_base_ime',
        '../base/ui_base.gyp:ui_base',
        '../display/display.gyp:display',
        '../events/events.gyp:events',
        '../events/events.gyp:events_base',
        '../events/events.gyp:gesture_detection',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        '../message_center/message_center.gyp:message_center',
        '../views/views.gyp:views',
        '../wm/wm.gyp:wm',
        'ui_chromeos_resources',
        'ui_chromeos_strings',
      ],
      'defines': [
        'UI_CHROMEOS_IMPLEMENTATION',
      ],
      'sources': [
        'accelerometer/accelerometer_util.cc',
        'accelerometer/accelerometer_util.h',
        'ime/candidate_view.cc',
        'ime/candidate_view.h',
        'ime/candidate_window_view.cc',
        'ime/candidate_window_view.h',
        'ime/infolist_window.cc',
        'ime/infolist_window.h',
        'ime/input_method_menu_item.cc',
        'ime/input_method_menu_item.h',
        'ime/input_method_menu_manager.cc',
        'ime/input_method_menu_manager.h',
        'ime/mode_indicator_view.cc',
        'ime/mode_indicator_view.h',
        'material_design_icon_controller.cc',
        'material_design_icon_controller.h',
        'network/network_connect.cc',
        'network/network_connect.h',
        'network/network_icon.cc',
        'network/network_icon.h',
        'network/network_icon_animation.cc',
        'network/network_icon_animation.h',
        'network/network_icon_animation_observer.h',
        'network/network_info.cc',
        'network/network_info.h',
        'network/network_list.cc',
        'network/network_list.h',
        'network/network_list_delegate.h',
        'network/network_list_view_base.cc',
        'network/network_list_view_base.h',
        'network/network_state_notifier.cc',
        'network/network_state_notifier.h',
        'touch_exploration_controller.cc',
        'touch_exploration_controller.h',
        'user_activity_power_manager_notifier.cc',
        'user_activity_power_manager_notifier.h',
      ],
    },  # target_name: ui_chromeos
    {
      'target_name': 'ui_chromeos_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../../base/base.gyp:test_support_base',
        '../../chromeos/chromeos.gyp:chromeos',
        '../../skia/skia.gyp:skia',
        '../../testing/gtest.gyp:gtest',
        '../aura/aura.gyp:aura_test_support',
        '../compositor/compositor.gyp:compositor',
        '../message_center/message_center.gyp:message_center',
        '../resources/ui_resources.gyp:ui_test_pak',
        '../views/views.gyp:views',
        '../views/views.gyp:views_test_support',
        'ui_chromeos',
      ],
      'sources': [
        '../chromeos/ime/candidate_view_unittest.cc',
        '../chromeos/ime/candidate_window_view_unittest.cc',
        '../chromeos/ime/input_method_menu_item_unittest.cc',
        '../chromeos/ime/input_method_menu_manager_unittest.cc',
        '../chromeos/network/network_state_notifier_unittest.cc',
        '../chromeos/touch_exploration_controller_unittest.cc',
        'run_all_unittests.cc',
      ],
    },  # target_name: ui_chromeos_unittests
  ],
}
