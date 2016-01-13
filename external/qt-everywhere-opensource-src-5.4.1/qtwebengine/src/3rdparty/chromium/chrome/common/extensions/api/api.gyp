# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  # All files are stored in these lists which are referenced in the target
  # below so that the GN build of this target can read in this dictionary and
  # duplicate the same logic without the lists getting out-of-sync. The GN
  # .gypi reader can not process conditions and does not know about targets,
  # etc., it just reads Python dictionaries.
  #
  # If you add a new category, also add it to the BUILD.gn file in this
  # directory.
  'variables': {
    # These duplicate other lists and are the only ones used on Android. They
    # should be eliminated. See crbug.com/305852.
    'android_schema_files': [
      'activity_log_private.json',
      'events.json',
      'file_system.idl',
      'manifest_types.json',
      'permissions.json',
      'sync_file_system.idl',
      'tab_capture.idl',
      'tabs.json',
      'types.json',
      'web_navigation.json',
      'webview.json',
      'windows.json',
    ],

    # These are used everywhere except Android.
    'main_schema_files': [
      'accessibility_private.json',
      'activity_log_private.json',
      'alarms.idl',
      'app_current_window_internal.idl',
      'app_window.idl',
      'audio.idl',
      'automation.idl',
      'automation_internal.idl',
      'autotest_private.idl',
      'bluetooth.idl',
      'bluetooth_low_energy.idl',
      'bluetooth_private.json',
      'bluetooth_socket.idl',
      'bookmark_manager_private.json',
      'bookmarks.json',
      'braille_display_private.idl',
      'browser.idl',
      'cast_channel.idl',
      'cloud_print_private.json',
      'command_line_private.json',
      'content_settings.json',
      'context_menus_internal.json',
      'context_menus.json',
      'cookies.json',
      'debugger.json',
      'desktop_capture.json',
      'developer_private.idl',
      'dial.idl',
      'downloads.idl',
      'downloads_internal.idl',
      'echo_private.json',
      'enterprise_platform_keys_private.json',
      'events.json',
      'feedback_private.idl',
      'file_browser_private.idl',
      'file_browser_private_internal.idl',
      'file_system.idl',
      'file_system_provider.idl',
      'file_system_provider_internal.idl',
      'font_settings.json',
      'gcd_private.idl',
      'gcm.json',
      'guest_view_internal.json',
      'hangouts_private.idl',
      'hid.idl',
      'history.json',
      'hotword_private.idl',
      'i18n.json',
      'identity.idl',
      'identity_private.idl',
      'idle.json',
      'image_writer_private.idl',
      'input_ime.json',
      'location.idl',
      'management.json',
      'manifest_types.json',
      'mdns.idl',
      'media_galleries.idl',
      'media_galleries_private.idl',
      'metrics_private.json',
      'networking_private.json',
      'notifications.idl',
      'omnibox.json',
      'page_capture.json',
      'permissions.json',
      'power.idl',
      'preferences_private.json',
      'push_messaging.idl',
      'reading_list_private.json',
      'screenlock_private.idl',
      'serial.idl',
      'sessions.json',
      'signed_in_devices.idl',
      'streams_private.idl',
      'synced_notifications_private.idl',
      'sync_file_system.idl',
      'system_cpu.idl',
      'system_display.idl',
      'system_indicator.idl',
      'system_memory.idl',
      'system_network.idl',
      'system_private.json',
      'system_storage.idl',
      'tab_capture.idl',
      'tabs.json',
      'terminal_private.json',
      'types.json',
      'virtual_keyboard_private.json',
      'web_navigation.json',
      'web_request.json',
      # Despite the name, this API does not rely on any
      # WebRTC-specific bits and as such does not belong in
      # the enable_webrtc=0 section below.
      'webrtc_audio_private.idl',
      'webrtc_logging_private.idl',
      'webstore_private.json',
      'webview.json',
      'windows.json',
    ],
    'main_non_compiled_schema_files': [
      'browsing_data.json',
      'chromeos_info_private.json',
      'extension.json',
      'idltest.idl',
      'infobars.json',
      'media_player_private.json',
      'music_manager_private.idl',
      'principals_private.idl',
      'top_sites.json',
      'web_request_internal.json',
    ],

    # ChromeOS-specific schemas.
    'chromeos_schema_files': [
      'accessibility_features.json',
      'diagnostics.idl',
      'enterprise_platform_keys.idl',
      'enterprise_platform_keys_internal.idl',
      'file_browser_handler_internal.json',
      'first_run_private.json',
      'log_private.idl',
      'wallpaper.json',
      'wallpaper_private.json',
      'webcam_private.idl',
    ],
    'chromeos_branded_schema_files': [
      'ledger/ledger.idl',
    ],

    'webrtc_schema_files': [
      'cast_streaming_rtp_stream.idl',
      'cast_streaming_session.idl',
      'cast_streaming_udp_transport.idl',
    ],
  },
  'targets': [
    {
      # GN version: //chrome/common/extensions/api:api
      'target_name': 'chrome_api',
      'type': 'static_library',
      'sources': [
        '<@(schema_files)',
      ],
      # TODO(jschuh): http://crbug.com/167187 size_t -> int
      'msvs_disabled_warnings': [ 4267 ],
      'includes': [
        '../../../../build/json_schema_bundle_compile.gypi',
        '../../../../build/json_schema_compile.gypi',
      ],
      'variables': {
        'chromium_code': 1,
        # Disable schema compiler to generate model extension API code.
        # Only register the extension functions in extension system.
        'conditions': [
          ['OS!="android"', {
            'non_compiled_schema_files': [
              '<@(main_non_compiled_schema_files)',
            ],
            'schema_files': [
              '<@(main_schema_files)',
            ],
          }, {  # OS=="android"
            'non_compiled_schema_files': [
            ],
            'schema_files': [
              # These should be eliminated. See crbug.com/305852.
              '<@(android_schema_files)',
            ],
          }],
          ['chromeos==1', {
            'schema_files': [
              '<@(chromeos_schema_files)',
            ],
          }],
          ['enable_webrtc==1', {
            'schema_files': [
              '<@(webrtc_schema_files)',
            ],
          }],
          ['branding=="Chrome" and chromeos==1', {
            'schema_files': [
              '<@(chromeos_branded_schema_files)',
            ],
          }],
        ],
        'cc_dir': 'chrome/common/extensions/api',
        'root_namespace': 'extensions::api',
      },
      'dependencies': [
        # Different APIs include some headers crom chrome/common that in turn
        # include generated headers from these targets.
        # TODO(brettw) this should be made unnecessary if possible.
        '<(DEPTH)/components/components.gyp:component_metrics_proto',
        '<(DEPTH)/device/serial/serial.gyp:device_serial',

        '<(DEPTH)/content/content.gyp:content_browser',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/sync/sync.gyp:sync',
      ],
      'conditions': [
        ['chromeos==1', {
          'dependencies': [
            '<(DEPTH)/chrome/chrome.gyp:drive_proto',
          ],
        }],
      ],
    },
  ],
}
