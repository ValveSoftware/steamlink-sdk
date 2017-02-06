# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # Core sources shared by sessions_content and sessions_ios. These can't
    # be a separate shared library since one symbol is implemented higher up in
    # the sessions_content/ios layer.
    'sessions_core_sources': [
      'sessions/core/base_session_service.cc',
      'sessions/core/base_session_service.h',
      'sessions/core/base_session_service_commands.cc',
      'sessions/core/base_session_service_commands.h',
      'sessions/core/base_session_service_delegate.h',
      'sessions/core/live_tab.cc',
      'sessions/core/live_tab.h',
      'sessions/core/live_tab_context.h',
      'sessions/core/persistent_tab_restore_service.cc',
      'sessions/core/persistent_tab_restore_service.h',
      'sessions/core/serialized_navigation_driver.h',
      'sessions/core/serialized_navigation_entry.cc',
      'sessions/core/serialized_navigation_entry.h',
      'sessions/core/session_backend.cc',
      'sessions/core/session_backend.h',
      'sessions/core/session_command.cc',
      'sessions/core/session_command.h',
      'sessions/core/session_constants.cc',
      'sessions/core/session_constants.h',
      'sessions/core/session_id.cc',
      'sessions/core/session_id.h',
      'sessions/core/session_service_commands.cc',
      'sessions/core/session_service_commands.h',
      'sessions/core/session_types.cc',
      'sessions/core/session_types.h',
      'sessions/core/tab_restore_service.cc',
      'sessions/core/tab_restore_service.h',
      'sessions/core/tab_restore_service_client.cc',
      'sessions/core/tab_restore_service_client.h',
      'sessions/core/tab_restore_service_helper.cc',
      'sessions/core/tab_restore_service_helper.h',
      'sessions/core/tab_restore_service_observer.h',
    ],
  },
  'targets': [
    {
      # GN version: //components/sessions:test_support
      'target_name': 'sessions_test_support',
      'type': 'static_library',
      'dependencies': [
        '../skia/skia.gyp:skia',
        '../sync/sync.gyp:sync',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'sessions/core/serialized_navigation_entry_test_helper.cc',
        'sessions/core/serialized_navigation_entry_test_helper.h',
      ],
      'conditions': [
        ['OS!="ios" and OS!="android"', {
          'sources': [
            'sessions/core/base_session_service_test_helper.cc',
            'sessions/core/base_session_service_test_helper.h',
           ],
        }],
      ],
    },
  ],

  # Platform-specific targets.
  'conditions': [
    ['OS!="ios"', {
      'targets': [
        {
          # GN version: //components/sessions
          'target_name': 'sessions_content',
          'type': '<(component)',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
            '../content/content.gyp:content_browser',
            '../skia/skia.gyp:skia',
            '../sync/sync.gyp:sync',
            '../third_party/protobuf/protobuf.gyp:protobuf_lite',
            '../ui/base/ui_base.gyp:ui_base',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            '../url/url.gyp:url_lib',
            'keyed_service_core',
            'variations',
          ],
          'include_dirs': [
            '..',
          ],
          'defines': [
            'SESSIONS_IMPLEMENTATION',
          ],
          'sources': [
            # Note: sources list duplicated in GN build.
            '<@(sessions_core_sources)',

            'sessions/content/content_live_tab.cc',
            'sessions/content/content_live_tab.h',
            'sessions/content/content_platform_specific_tab_data.cc',
            'sessions/content/content_platform_specific_tab_data.h',
            'sessions/content/content_serialized_navigation_builder.cc',
            'sessions/content/content_serialized_navigation_builder.h',
            'sessions/content/content_serialized_navigation_driver.cc',
            'sessions/content/content_serialized_navigation_driver.h',
          ],
          'conditions': [
            ['OS=="android"', {
             'sources': [
               'sessions/core/in_memory_tab_restore_service.cc',
               'sessions/core/in_memory_tab_restore_service.h',
              ],
              'sources!': [
               'sessions/core/persistent_tab_restore_service.cc',
              ],
            },
          ],
        ],
      }],
    }, {  # OS==ios
      'targets': [
        {
          # GN version: //components/sessions
          'target_name': 'sessions_ios',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../ios/web/ios_web.gyp:ios_web',
            '../sync/sync.gyp:sync',
            '../third_party/protobuf/protobuf.gyp:protobuf_lite',
            '../ui/base/ui_base.gyp:ui_base',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            '../url/url.gyp:url_lib',
            'keyed_service_core',
            'variations',
          ],
          'include_dirs': [
            '..',
          ],
          'defines': [
            'SESSIONS_IMPLEMENTATION',
          ],
          'sources': [
            '<@(sessions_core_sources)',

            'sessions/ios/ios_live_tab.h',
            'sessions/ios/ios_live_tab.mm',
            'sessions/ios/ios_serialized_navigation_builder.h',
            'sessions/ios/ios_serialized_navigation_builder.mm',
            'sessions/ios/ios_serialized_navigation_driver.cc',
            'sessions/ios/ios_serialized_navigation_driver.h',
          ],
        },
      ],
    }],
  ],

}
