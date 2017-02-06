# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN: //ui/arc:arc
      'target_name': 'arc',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'dependencies': [
        '../aura/aura.gyp:aura',
        '../base/ui_base.gyp:ui_base',
        '../compositor/compositor.gyp:compositor',
        '../display/display.gyp:display',
        '../events/events.gyp:events',
        '../gfx/gfx.gyp:gfx_geometry',
        '../message_center/message_center.gyp:message_center',
        '../resources/ui_resources.gyp:ui_resources',
        '../strings/ui_strings.gyp:ui_strings',
        '../views/views.gyp:views',
        '../wm/wm.gyp:wm',
        '../../ash/ash.gyp:ash',
        '../../base/base.gyp:base',
        '../../url/url.gyp:url_lib',
        '../../skia/skia.gyp:skia',
        '../../components/components.gyp:arc_base',
        '../../components/components.gyp:arc_bitmap',
        '../../components/components.gyp:arc_mojo_bindings',
        '../../components/components.gyp:exo',
        '../../components/components.gyp:signin_core_account_id',
      ],
      'sources': [
        'notification/arc_custom_notification_view.cc',
        'notification/arc_custom_notification_view.h',
        'notification/arc_custom_notification_item.cc',
        'notification/arc_custom_notification_item.h',
        'notification/arc_notification_item.cc',
        'notification/arc_notification_item.h',
        'notification/arc_notification_manager.cc',
        'notification/arc_notification_manager.h',
        'notification/arc_notification_surface_manager.cc',
        'notification/arc_notification_surface_manager.h',
      ],
    },
    {
      # GN: //ui/arc:ui_arc_unittests
      'target_name': 'ui_arc_unittests',
      'type': '<(gtest_target_type)',
      'include_dirs': [
        '../..',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../components/components.gyp:arc_test_support',
        '../../mojo/mojo_edk.gyp:mojo_system_impl',
        '../../mojo/mojo_public.gyp:mojo_cpp_bindings',
        '../../testing/gtest.gyp:gtest',
        '../message_center/message_center.gyp:message_center_test_support',
        'arc',
      ],
      'sources': [
        'notification/arc_notification_manager_unittest.cc',
        'test/run_all_unittests.cc',
      ],
    },
  ],
}
