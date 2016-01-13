# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'message_center',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:base_i18n',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../skia/skia.gyp:skia',
        '../../url/url.gyp:url_lib',
        '../base/ui_base.gyp:ui_base',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        '../resources/ui_resources.gyp:ui_resources',
        '../strings/ui_strings.gyp:ui_strings',
      ],
      'defines': [
        'MESSAGE_CENTER_IMPLEMENTATION',
      ],
      'sources': [
        'cocoa/notification_controller.h',
        'cocoa/notification_controller.mm',
        'cocoa/opaque_views.h',
        'cocoa/opaque_views.mm',
        'cocoa/popup_collection.h',
        'cocoa/popup_collection.mm',
        'cocoa/popup_controller.h',
        'cocoa/popup_controller.mm',
        'cocoa/settings_controller.h',
        'cocoa/settings_controller.mm',
        'cocoa/settings_entry_view.h',
        'cocoa/settings_entry_view.mm',
        'cocoa/status_item_view.h',
        'cocoa/status_item_view.mm',
        'cocoa/tray_controller.h',
        'cocoa/tray_controller.mm',
        'cocoa/tray_view_controller.h',
        'cocoa/tray_view_controller.mm',
        'dummy_message_center.cc',
        'message_center.cc',
        'message_center.h',
        'message_center_export.h',
        'notification_delegate.cc',
        'notification_delegate.h',
        'message_center_impl.cc',
        'message_center_impl.h',
        'message_center_observer.h',
        'message_center_style.cc',
        'message_center_style.h',
        'message_center_tray.cc',
        'message_center_tray.h',
        'message_center_tray_delegate.h',
        'message_center_types.h',
        'notification.cc',
        'notification.h',
        'notification_blocker.cc',
        'notification_blocker.h',
        'notification_list.cc',
        'notification_list.h',
        'notification_types.cc',
        'notification_types.h',
        'notifier_settings.cc',
        'notifier_settings.h',
        'views/bounded_label.cc',
        'views/bounded_label.h',
        'views/constants.h',
        'views/message_bubble_base.cc',
        'views/message_bubble_base.h',
        'views/message_center_controller.h',
        'views/message_center_bubble.cc',
        'views/message_center_bubble.h',
        'views/message_center_button_bar.cc',
        'views/message_center_button_bar.h',
        'views/message_center_view.cc',
        'views/message_center_view.h',
        'views/message_popup_collection.cc',
        'views/message_popup_collection.h',
        'views/message_view.cc',
        'views/message_view.h',
        'views/message_view_context_menu_controller.cc',
        'views/message_view_context_menu_controller.h',
        'views/notifier_settings_view.cc',
        'views/notifier_settings_view.h',
        'views/notification_button.cc',
        'views/notification_button.h',
        'views/notification_view.cc',
        'views/notification_view.h',
        'views/padded_button.cc',
        'views/padded_button.h',
        'views/proportional_image_view.cc',
        'views/proportional_image_view.h',
        'views/toast_contents_view.cc',
        'views/toast_contents_view.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
      'conditions': [
        # This condition is for Windows 8 Metro mode support.  We need to
        # specify a particular desktop during widget creation in that case.
        # This is done using the desktop aura native widget framework.
        ['use_ash==1 and OS=="win"', {
          'dependencies': [
            '../aura/aura.gyp:aura',
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../events/events.gyp:events',
            '../views/views.gyp:views',
            '../compositor/compositor.gyp:compositor',
          ],
        }, {
          'sources/': [
            ['exclude', 'views/'],
          ],
        }],
        ['use_ash==0', {
          'sources!': [
            'views/message_bubble_base.cc',
            'views/message_bubble_base.h',
            'views/message_center_bubble.cc',
            'views/message_center_bubble.h',
            'views/message_popup_bubble.cc',
            'views/message_popup_bubble.h',
          ],
        }],
        # iOS disables notifications altogether, Android implements its own
        # notification UI manager instead of deferring to the message center.
        ['notifications==0 or OS=="android"', {
          'sources/': [
            # Exclude everything except dummy impl.
            ['exclude', '\\.(cc|mm)$'],
            ['include', '^dummy_message_center\\.cc$'],
            ['include', '^message_center_switches\\.cc$'],
          ],
        }, {  # notifications==1
          'sources!': [ 'dummy_message_center.cc' ],
        }],
        # Include a minimal set of files required for notifications on Android.
        ['OS=="android"', {
          'sources/': [
            ['include', '^notification\\.cc$'],
            ['include', '^notification_delegate\\.cc$'],
            ['include', '^notifier_settings\\.cc$'],
          ],
        }],
      ],
    },  # target_name: message_center
    {
      'target_name': 'message_center_test_support',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../skia/skia.gyp:skia',
        '../base/ui_base.gyp:ui_base',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        'message_center',
      ],
      'sources': [
        'fake_message_center.h',
        'fake_message_center.cc',
        'fake_message_center_tray_delegate.h',
        'fake_message_center_tray_delegate.cc',
        'fake_notifier_settings_provider.h',
        'fake_notifier_settings_provider.cc',
      ],
    },  # target_name: message_center_test_support
    {
      'target_name': 'message_center_unittests',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../skia/skia.gyp:skia',
        '../../testing/gtest.gyp:gtest',
        '../../url/url.gyp:url_lib',
        '../base/ui_base.gyp:ui_base',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        '../resources/ui_resources.gyp:ui_resources',
        '../resources/ui_resources.gyp:ui_test_pak',
        'message_center',
        'message_center_test_support',
      ],
      'sources': [
        'cocoa/notification_controller_unittest.mm',
        'cocoa/popup_collection_unittest.mm',
        'cocoa/popup_controller_unittest.mm',
        'cocoa/settings_controller_unittest.mm',
        'cocoa/status_item_view_unittest.mm',
        'cocoa/tray_controller_unittest.mm',
        'cocoa/tray_view_controller_unittest.mm',
        'message_center_tray_unittest.cc',
        'message_center_impl_unittest.cc',
        'notification_delegate_unittest.cc',
        'notification_list_unittest.cc',
        'test/run_all_unittests.cc',
      ],
      'conditions': [
        ['OS=="mac"', {
          'dependencies': [
            '../gfx/gfx.gyp:gfx_test_support',
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            # Compositor is needed by message_center_view_unittest.cc
            # and for the fonts used by bounded_label_unittest.cc.
            '../compositor/compositor.gyp:compositor',
            '../views/views.gyp:views',
            '../views/views.gyp:views_test_support',
          ],
          'sources': [
            'views/bounded_label_unittest.cc',
            'views/message_center_view_unittest.cc',
            'views/message_popup_collection_unittest.cc',
            'views/notification_view_unittest.cc',
            'views/notifier_settings_view_unittest.cc',
          ],
        }],
        ['notifications==0', {  # Android and iOS.
          'sources/': [
            # Exclude everything except main().
            ['exclude', '\\.(cc|mm)$'],
            ['include', '^test/run_all_unittests\\.cc$'],
          ],
        }],
        # See http://crbug.com/162998#c4 for why this is needed.
        ['OS=="linux" and use_allocator!="none"', {
          'dependencies': [
            '../../base/allocator/allocator.gyp:allocator',
          ],
        }],
      ],
    },  # target_name: message_center_unittests
  ],
}
