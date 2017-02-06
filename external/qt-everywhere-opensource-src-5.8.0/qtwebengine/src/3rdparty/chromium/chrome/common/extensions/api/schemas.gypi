# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'sources': [
    '<@(schema_files)',
  ],
  'variables': {
    'main_schema_files': [
      'accessibility_features.json',
      'accessibility_private.json',
      'activity_log_private.json',
      'audio_modem.idl',
      'autofill_private.idl',
      'automation.idl',
      'automation_internal.idl',
      'autotest_private.idl',
      'bluetooth_low_energy.idl',
      'bookmark_manager_private.json',
      'bookmarks.json',
      'braille_display_private.idl',
      'browser.idl',
      'chrome_web_view_internal.json',
      'cloud_print_private.json',
      'command_line_private.json',
      'content_settings.json',
      'context_menus_internal.json',
      'context_menus.json',
      'cookies.json',
      'copresence.idl',
      'copresence_private.idl',
      'cryptotoken_private.idl',
      'dashboard_private.json',
      'data_reduction_proxy.json',
      'debugger.json',
      'desktop_capture.json',
      'developer_private.idl',
      'dial.idl',
      'downloads.idl',
      'downloads_internal.idl',
      'easy_unlock_private.idl',
      'experience_sampling_private.json',
      'feedback_private.idl',
      'file_system.idl',
      'font_settings.json',
      'gcm.json',
      'history.json',
      'hotword_private.idl',
      'i18n.json',
      'identity.idl',
      'identity_private.idl',
      'image_writer_private.idl',
      'inline_install_private.idl',
      'instance_id.json',
      'language_settings_private.idl',
      'manifest_types.json',
      'media_galleries.idl',
      'metrics_private.json',
      'notification_provider.idl',
      'notifications.idl',
      'omnibox.json',
      'page_capture.json',
      'passwords_private.idl',
      'permissions.json',
      'preferences_private.json',
      'quick_unlock_private.idl',
      'resources_private.idl',
      'screenlock_private.idl',
      'sessions.json',
      'settings_private.idl',
      'signed_in_devices.idl',
      'streams_private.idl',
      'sync_file_system.idl',
      'system_indicator.idl',
      'system_private.json',
      'tab_capture.idl',
      'tabs.json',
      'types.json',
      'web_navigation.json',
      # Despite the name, this API does not rely on any
      # WebRTC-specific bits and as such does not belong in
      # the enable_webrtc==0 section below.
      'webrtc_audio_private.idl',
      'webrtc_desktop_capture_private.idl',
      'webrtc_logging_private.idl',
      'webstore_private.json',
      'webstore_widget_private.idl',
      'windows.json',
    ],
    'main_schema_include_rules': [
      'extensions/common/api:extensions::api::%(namespace)s',
    ],
    'main_non_compiled_schema_files': [
      'browsing_data.json',
      'extension.json',
      'idltest.idl',
      'music_manager_private.idl',
      'top_sites.json',
    ],

    # ChromeOS-specific schemas.
    'chromeos_schema_files': [
      'cast_devices_private.idl',
      'certificate_provider.idl',
      'certificate_provider_internal.idl',
      'echo_private.json',
      'enterprise_device_attributes.idl',
      'enterprise_platform_keys.idl',
      'enterprise_platform_keys_internal.idl',
      'enterprise_platform_keys_private.json',
      'file_browser_handler_internal.json',
      'file_manager_private.idl',
      'file_manager_private_internal.idl',
      'file_system_provider.idl',
      'file_system_provider_internal.idl',
      'first_run_private.json',
      'input_ime.json',
      'input_method_private.json',
      'launcher_search_provider.idl',
      'log_private.idl',
      'platform_keys.idl',
      'platform_keys_internal.idl',
      'terminal_private.json',
      'users_private.idl',
      'wallpaper.json',
      'wallpaper_private.json',
    ],
    'chromeos_non_compiled_schema_files': [
      'chromeos_info_private.json',
      'media_player_private.json',
    ],

    'service_discovery_schema_files': [
      'gcd_private.idl',
      'mdns.idl',
    ],

    'webrtc_schema_files': [
      'cast_streaming_receiver_session.idl',
      'cast_streaming_rtp_stream.idl',
      'cast_streaming_session.idl',
      'cast_streaming_udp_transport.idl',
    ],
    'app_list_schema_files': [
      # TODO(tapted): Move this into chromeos_schema_files.
      'launcher_page.idl',
    ],

    'task_manager_dependent_schema_files': [
      'processes.idl',
    ],

    # Input IME schema.
    'input_ime_schema_file': [
      'input_ime.json',
    ],

    'non_compiled_schema_files': [
      '<@(main_non_compiled_schema_files)',
    ],
    'schema_dependencies': [
      '<(DEPTH)/extensions/common/api/api.gyp:extensions_api',
    ],
    'schema_files': [
      '<@(main_schema_files)',
    ],
    'schema_include_rules': [
      '<@(main_schema_include_rules)',
    ],

    'chromium_code': 1,
    # Disable schema compiler to generate model extension API code.
    # Only register the extension functions in extension system.
    'conditions': [
      ['enable_task_manager==1', {
        'schema_files': [
          '<@(task_manager_dependent_schema_files)',
        ],
      }],
      ['chromeos==1', {
        'schema_files': [
          '<@(chromeos_schema_files)',
        ],
        'non_compiled_schema_files': [
          '<@(chromeos_non_compiled_schema_files)',
        ],
      }, { # chromeos==0
        'conditions': [
          ['OS=="linux" or OS=="win"', {
            'schema_files': [
              '<@(input_ime_schema_file)',
            ],
          }],
        ],
      }],
      ['enable_service_discovery==1', {
        'schema_files': [
          '<@(service_discovery_schema_files)',
        ],
      }],
      ['enable_webrtc==1', {
        'schema_files': [
          '<@(webrtc_schema_files)',
        ],
      }],
      ['enable_app_list==1', {
        'schema_files': [
          '<@(app_list_schema_files)',
        ],
      }],
    ],
    'cc_dir': 'chrome/common/extensions/api',
    'root_namespace': 'extensions::api::%(namespace)s',
    'bundle_name': 'Chrome',
    'impl_dir_': 'chrome/browser/extensions/api',
  },
}
