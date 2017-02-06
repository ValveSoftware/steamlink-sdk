# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'extensions.gypi',
  ],
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //extensions/common
      'target_name': 'extensions_common_constants',
      'type': 'static_library',
      'include_dirs': [
        '..',
        '<(INTERMEDIATE_DIR)',
      ],
      'sources': [
        '<@(extensions_common_constants_sources)',
      ],
      # Disable c4267 warnings until we fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      # GN version: //extensions/common:mojo
      'target_name': 'extensions_common_mojo',
      # The type of this target must be none. This is so that resources can
      # depend upon this target for generating the js bindings files. Any
      # generated cpp files must be listed explicitly in extensions_common
      'type': 'none',
      'includes': [
        '../mojo/mojom_bindings_generator.gypi',
      ],
      'sources': [
        '<@(extensions_common_mojo_sources)',
      ],
      'conditions': [
        ['proprietary_codecs==1 and enable_wifi_display==1', {
          'sources': [
            '<@(extensions_common_mojo_sources_wifi_display)',
          ],
        }],
      ],
    },
    {
      # GN version: //extensions/common
      'target_name': 'extensions_common',
      'type': 'static_library',
      'dependencies': [
        '../components/components.gyp:crx_file',
        '../components/components.gyp:url_matcher',
        '../content/content.gyp:content_common',
        '../crypto/crypto.gyp:crypto',
        '../device/bluetooth/bluetooth.gyp:device_bluetooth',
        # For Mojo generated headers for generated_api.cc.
        '../device/serial/serial.gyp:device_serial_mojo',
        '../device/power_save_blocker/power_save_blocker.gyp:device_power_save_blocker',
        '../device/usb/usb.gyp:device_usb',
        '../ipc/ipc.gyp:ipc',
        '../net/net.gyp:net',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/re2/re2.gyp:re2',
        '../ui/base/ui_base.gyp:ui_base',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../ui/gfx/ipc/geometry/gfx_ipc_geometry.gyp:gfx_ipc_geometry',
        '../ui/gfx/ipc/gfx_ipc.gyp:gfx_ipc',
        '../ui/gfx/ipc/skia/gfx_ipc_skia.gyp:gfx_ipc_skia',
        '../url/url.gyp:url_lib',
        '../third_party/boringssl/boringssl.gyp:boringssl',
        '../third_party/libxml/libxml.gyp:libxml',
        '../url/ipc/url_ipc.gyp:url_ipc',
        'common/api/api.gyp:extensions_api',
        'extensions_resources.gyp:extensions_resources',
        'extensions_strings.gyp:extensions_strings',
        'extensions_common_constants',
        'extensions_common_mojo',
      ],
      'include_dirs': [
        '..',
        '<(INTERMEDIATE_DIR)',
      ],
      'sources': [
        '<@(extensions_common_sources)',

        # Mojom generated files should not be included in the common gypi source
        # list because GN has proper mojom dependency support.
        '<(SHARED_INTERMEDIATE_DIR)/extensions/common/mojo/keep_alive.mojom.cc',
        '<(SHARED_INTERMEDIATE_DIR)/extensions/common/mojo/keep_alive.mojom.h',
        '<(SHARED_INTERMEDIATE_DIR)/extensions/common/mojo/stash.mojom.cc',
        '<(SHARED_INTERMEDIATE_DIR)/extensions/common/mojo/stash.mojom.h',
      ],
      # Disable c4267 warnings until we fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
      'conditions': [
        ['disable_nacl==0', {
          # NaClModulesHandler does not use any code in NaCl, so no dependency
          # on nacl_common.
          'sources': [
            '<@(extensions_common_sources_nacl)',
          ],
        }],
      ],
    },
    {
      # GN version: //extensions/browser
      'target_name': 'extensions_browser',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../components/components.gyp:browsing_data',
        '../components/components.gyp:cast_certificate',
        '../components/components.gyp:device_event_log_component',
        '../components/components.gyp:guest_view_browser',
        '../components/components.gyp:keyed_service_content',
        '../components/components.gyp:keyed_service_core',
        '../components/components.gyp:onc_component',
        '../components/components.gyp:pref_registry',
        '../components/components.gyp:sessions_content',
        '../components/components.gyp:storage_monitor',
        '../components/components.gyp:update_client',
        '../components/components.gyp:variations',
        '../components/components.gyp:version_info',
        '../components/components.gyp:web_cache_browser',
        '../components/components.gyp:web_modal',
        '../components/components.gyp:zoom',
        '../components/prefs/prefs.gyp:prefs',
        '../content/content.gyp:content_browser',
        '../device/bluetooth/bluetooth.gyp:device_bluetooth',
        '../device/serial/serial.gyp:device_serial',
        '../google_apis/google_apis.gyp:google_apis',
        '../skia/skia.gyp:skia',
        '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        '../third_party/re2/re2.gyp:re2',
        'browser/api/api_registration.gyp:extensions_api_registration',
        'common/api/api.gyp:cast_channel_proto',
        'common/api/api.gyp:extensions_api',
        'extensions_common',
        'extensions_resources.gyp:extensions_resources',
        'extensions_strings.gyp:extensions_strings',
      ],
      'include_dirs': [
        '..',
        '<(INTERMEDIATE_DIR)',
        # Needed to access generated API headers.
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'sources': [
        '<@(extensions_browser_sources)',
      ],
      'conditions': [
        # This condition exists only because the extensions_common_constants
        # target is always built and thus this file gets evaluated by GYP.
        # This does not need to be replicated into extensions/browser/BUILD.gn.
        ['OS == "ios" or OS == "android"', {
          'dependencies!': [
            '../components/components.gyp:storage_monitor',
          ],
        }],
        ['chromeos == 1', {
          'dependencies': [
            '../chromeos/chromeos.gyp:chromeos',
          ],
          'sources': [
            '<@(extensions_browser_sources_chromeos)',
          ],
        }, {  # chromeos==0
          'sources': [
            '<@(extensions_browser_sources_nonchromeos)',
          ],
        }],
        ['OS == "win" or OS == "mac"', {
          'dependencies': [
            '../components/components.gyp:wifi_component',
          ],
          'sources': [
            '<@(extensions_browser_sources_win_or_mac)',
          ],
        }],
        ['OS == "linux" and chromeos == 0', {
          'dependencies': [
            '../build/linux/system.gyp:dbus',
            '../dbus/dbus.gyp:dbus',
          ],
          'sources': [
            '<@(extensions_browser_sources_linux_nonchromeos)',
          ],
        }],
        ['proprietary_codecs==1 and enable_wifi_display == 1', {
          'sources': [
            '<@(extensions_browser_sources_wifi_display)',
            '<(SHARED_INTERMEDIATE_DIR)/extensions/common/mojo/wifi_display_session_service.mojom.cc',
            '<(SHARED_INTERMEDIATE_DIR)/extensions/common/mojo/wifi_display_session_service.mojom.h',
          ],
        }],
      ],
      # Disable c4267 warnings until we fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      # GN version: //extensions/renderer
      'target_name': 'extensions_renderer',
      'type': 'static_library',
      'dependencies': [
        'extensions_resources.gyp:extensions_resources',
        '../components/components.gyp:guest_view_renderer',
        '../content/content.gyp:content_common',
        '../content/content.gyp:content_resources',
        '../gin/gin.gyp:gin',
        '../mojo/mojo_public.gyp:mojo_js_bindings',
        '../third_party/WebKit/public/blink.gyp:blink',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '<@(extensions_renderer_sources)',
      ],
      # Disable c4267 warnings until we fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
      'conditions': [
        ['proprietary_codecs==1 and enable_wifi_display==1', {
          'dependencies': [
            '../third_party/openh264/openh264.gyp:openh264_encoder',
            '../third_party/wds/wds.gyp:libwds',
          ],
          'sources': [
            '<@(extensions_renderer_sources_wifi_display)',
          ],
        }],
      ],
    },
    {
      # GN version: //extensions/utility
      'target_name': 'extensions_utility',
      'type': 'static_library',
      'dependencies': [
        '../content/content.gyp:content_common',
        '../content/content.gyp:content_utility',
        '../skia/skia.gyp:skia',
        '../third_party/zlib/google/zip.gyp:zip',
        'extensions_common',
        'extensions_strings.gyp:extensions_strings',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '<@(extensions_utility_sources)',
      ],
    },
    {
      # GN version: //extensions:test_support
      'target_name': 'extensions_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../components/components.gyp:pref_registry_test_support',
        '../components/components.gyp:user_prefs',
        '../components/prefs/prefs.gyp:prefs_test_support',
        '../content/content.gyp:content_browser',
        '../content/content.gyp:content_common',
        '../content/content_shell_and_tests.gyp:test_support_content',
        '../net/net.gyp:net_test_support',
        '../testing/gtest.gyp:gtest',
        '../third_party/cld_2/cld_2.gyp:cld_2',
        'browser/api/api_registration.gyp:extensions_api_registration',
        'common/api/api.gyp:cast_channel_proto',
        'common/api/api.gyp:extensions_api',
        'extensions_browser',
        'extensions_common',
        'extensions_resources.gyp:extensions_resources',
      ],
      'include_dirs': [
        '..',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'sources': [
        '<@(extensions_test_support_sources)',
      ],
      # Disable c4267 warnings until we fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      # The pak file generated by this target is intended to be shared by
      # both shell and test targets. It was combined because it might help a
      # little bit with build time by avoiding a repack step (one instead of
      # two).
      'target_name': 'extensions_shell_and_test_pak',
      'type': 'none',
      'dependencies': [
        '../content/browser/devtools/devtools_resources.gyp:devtools_resources',
        '../content/content.gyp:content_resources',
        '../content/content_shell_and_tests.gyp:content_shell_resources',
        '../third_party/WebKit/public/blink_resources.gyp:blink_resources',
        '../ui/resources/ui_resources.gyp:ui_resources',
        '../ui/strings/ui_strings.gyp:ui_strings',
        'extensions_resources.gyp:extensions_resources',
        'extensions_strings.gyp:extensions_strings',
        'shell/app_shell_resources.gyp:app_shell_resources',
      ],
      'actions': [
        {
          'action_name': 'repack_extensions_shell_and_test_pak',
          'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/blink/public/resources/blink_image_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/blink/public/resources/blink_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/app/strings/content_strings_en-US.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/shell_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/extensions/extensions_browser_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/extensions/extensions_renderer_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/extensions/extensions_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/extensions/shell/app_shell_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/extensions/strings/extensions_strings_en-US.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/strings/app_locale_settings_en-US.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/strings/ui_strings_en-US.pak',
              '<(SHARED_INTERMEDIATE_DIR)/blink/devtools_resources.pak',
            ],
            'pak_output': '<(PRODUCT_DIR)/extensions_shell_and_test.pak',
          },
          'includes': [ '../build/repack_action.gypi' ],
        },
      ],
    },
  ]
}
