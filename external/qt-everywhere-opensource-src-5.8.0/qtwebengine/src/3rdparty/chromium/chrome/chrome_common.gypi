# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # File lists shared with GN build.
    'chrome_common_sources': [
      'common/all_messages.h',
      'common/attrition_experiments.h',
      'common/auto_start_linux.cc',
      'common/auto_start_linux.h',
      'common/channel_info.cc',
      'common/channel_info.h',
      'common/channel_info_android.cc',
      'common/channel_info_chromeos.cc',
      'common/channel_info_mac.mm',
      'common/channel_info_posix.cc',
      'common/channel_info_win.cc',
      'common/child_process_logging.h',
      'common/child_process_logging_win.cc',
      'common/chrome_content_client.cc',
      'common/chrome_content_client.h',
      'common/chrome_content_client_constants.cc',
      'common/chrome_content_client_ios.mm',
      'common/chrome_media_client_android.cc',
      'common/chrome_media_client_android.h',
      'common/chrome_result_codes.h',
      'common/chrome_utility_messages.h',
      'common/common_message_generator.cc',
      'common/common_message_generator.h',
      'common/common_param_traits.cc',
      'common/common_param_traits.h',
      'common/common_param_traits_macros.h',
      'common/component_flash_hint_file_linux.cc',
      'common/component_flash_hint_file_linux.h',
      'common/content_restriction.h',
      'common/content_settings_pattern_serializer.cc',
      'common/content_settings_pattern_serializer.h',
      'common/crash_keys.cc',
      'common/crash_keys.h',
      'common/custom_handlers/protocol_handler.cc',
      'common/custom_handlers/protocol_handler.h',
      'common/descriptors_android.h',
      'common/ini_parser.cc',
      'common/ini_parser.h',
      'common/instant_types.cc',
      'common/instant_types.h',
      'common/logging_chrome.cc',
      'common/logging_chrome.h',
      'common/mac/app_shim_launch.h',
      'common/mac/app_shim_messages.h',
      'common/mac/cfbundle_blocker.h',
      'common/mac/cfbundle_blocker.mm',
      'common/mac/launchd.h',
      'common/mac/launchd.mm',
      'common/media/media_resource_provider.cc',
      'common/media/media_resource_provider.h',
      'common/media/webrtc_logging_message_data.cc',
      'common/media/webrtc_logging_message_data.h',
      'common/media/webrtc_logging_messages.h',
      'common/media_galleries/metadata_types.h',
      'common/multi_process_lock.h',
      'common/multi_process_lock_linux.cc',
      'common/multi_process_lock_mac.cc',
      'common/multi_process_lock_win.cc',
      'common/origin_trials/chrome_origin_trial_policy.cc',
      'common/origin_trials/chrome_origin_trial_policy.h',
      'common/partial_circular_buffer.cc',
      'common/partial_circular_buffer.h',
      'common/pref_names_util.cc',
      'common/pref_names_util.h',
      'common/prerender_types.h',
      'common/profiling.cc',
      'common/profiling.h',
      'common/ref_counted_util.h',
      'common/render_messages.cc',
      'common/render_messages.h',
      'common/resource_usage_reporter_type_converters.cc',
      'common/resource_usage_reporter_type_converters.h',
      'common/safe_browsing/safebrowsing_messages.h',
      'common/search_provider.h',
      'common/search_types.h',
      'common/search_urls.cc',
      'common/search_urls.h',
      'common/secure_origin_whitelist.cc',
      'common/secure_origin_whitelist.h',
      'common/spellcheck_bdict_language.h',
      'common/spellcheck_common.cc',
      'common/spellcheck_common.h',
      'common/spellcheck_marker.h',
      'common/spellcheck_messages.h',
      'common/spellcheck_result.h',
      'common/ssl_insecure_content.cc',
      'common/ssl_insecure_content.h',
      'common/switch_utils.cc',
      'common/switch_utils.h',
      'common/trace_event_args_whitelist.cc',
      'common/trace_event_args_whitelist.h',
      'common/tts_messages.h',
      'common/tts_utterance_request.cc',
      'common/tts_utterance_request.h',
      'common/url_constants.cc',
      'common/url_constants.h',
      'common/v8_breakpad_support_win.cc',
      'common/v8_breakpad_support_win.h',
      'common/variations/child_process_field_trial_syncer.cc',
      'common/variations/child_process_field_trial_syncer.h',
      'common/variations/variations_util.cc',
      'common/variations/variations_util.h',
      'common/web_application_info.cc',
      'common/web_application_info.h',
      'common/widevine_cdm_constants.cc',
      'common/widevine_cdm_constants.h',
    ],
    'chrome_common_extensions_sources': [
      'common/cast_messages.cc',
      'common/cast_messages.h',
      'common/extensions/api/commands/commands_handler.cc',
      'common/extensions/api/commands/commands_handler.h',
      'common/extensions/api/extension_action/action_info.cc',
      'common/extensions/api/extension_action/action_info.h',
      'common/extensions/api/notifications/notification_style.cc',
      'common/extensions/api/notifications/notification_style.h',
      'common/extensions/api/omnibox/omnibox_handler.cc',
      'common/extensions/api/omnibox/omnibox_handler.h',
      'common/extensions/api/plugins/plugins_handler.cc',
      'common/extensions/api/plugins/plugins_handler.h',
      'common/extensions/api/speech/tts_engine_manifest_handler.cc',
      'common/extensions/api/speech/tts_engine_manifest_handler.h',
      'common/extensions/api/spellcheck/spellcheck_handler.cc',
      'common/extensions/api/spellcheck/spellcheck_handler.h',
      'common/extensions/api/storage/storage_schema_manifest_handler.cc',
      'common/extensions/api/storage/storage_schema_manifest_handler.h',
      'common/extensions/api/system_indicator/system_indicator_handler.cc',
      'common/extensions/api/system_indicator/system_indicator_handler.h',
      'common/extensions/api/url_handlers/url_handlers_parser.cc',
      'common/extensions/api/url_handlers/url_handlers_parser.h',
      'common/extensions/api/webstore/webstore_api_constants.cc',
      'common/extensions/api/webstore/webstore_api_constants.h',
      'common/extensions/chrome_extension_messages.h',
      'common/extensions/chrome_extensions_client.cc',
      'common/extensions/chrome_extensions_client.h',
      'common/extensions/chrome_manifest_handlers.cc',
      'common/extensions/chrome_manifest_handlers.h',
      'common/extensions/chrome_manifest_url_handlers.cc',
      'common/extensions/chrome_manifest_url_handlers.h',
      'common/extensions/chrome_utility_extensions_messages.h',
      'common/extensions/command.cc',
      'common/extensions/command.h',
      'common/extensions/extension_constants.cc',
      'common/extensions/extension_constants.h',
      'common/extensions/extension_metrics.cc',
      'common/extensions/extension_metrics.h',
      'common/extensions/extension_process_policy.cc',
      'common/extensions/extension_process_policy.h',
      'common/extensions/features/chrome_channel_feature_filter.cc',
      'common/extensions/features/chrome_channel_feature_filter.h',
      'common/extensions/features/feature_channel.cc',
      'common/extensions/features/feature_channel.h',
      'common/extensions/features/feature_util.cc',
      'common/extensions/features/feature_util.h',
      'common/extensions/image_writer/image_writer_util_mac.cc',
      'common/extensions/image_writer/image_writer_util_mac.h',
      'common/extensions/manifest_handlers/app_icon_color_info.cc',
      'common/extensions/manifest_handlers/app_icon_color_info.h',
      'common/extensions/manifest_handlers/app_launch_info.cc',
      'common/extensions/manifest_handlers/app_launch_info.h',
      'common/extensions/manifest_handlers/automation.cc',
      'common/extensions/manifest_handlers/automation.h',
      'common/extensions/manifest_handlers/content_scripts_handler.cc',
      'common/extensions/manifest_handlers/content_scripts_handler.h',
      'common/extensions/manifest_handlers/copresence_manifest.cc',
      'common/extensions/manifest_handlers/copresence_manifest.h',
      'common/extensions/manifest_handlers/extension_action_handler.cc',
      'common/extensions/manifest_handlers/extension_action_handler.h',
      'common/extensions/manifest_handlers/linked_app_icons.cc',
      'common/extensions/manifest_handlers/linked_app_icons.h',
      'common/extensions/manifest_handlers/minimum_chrome_version_checker.cc',
      'common/extensions/manifest_handlers/minimum_chrome_version_checker.h',
      'common/extensions/manifest_handlers/settings_overrides_handler.cc',
      'common/extensions/manifest_handlers/settings_overrides_handler.h',
      'common/extensions/manifest_handlers/theme_handler.cc',
      'common/extensions/manifest_handlers/theme_handler.h',
      'common/extensions/manifest_handlers/ui_overrides_handler.cc',
      'common/extensions/manifest_handlers/ui_overrides_handler.h',
      'common/extensions/permissions/chrome_api_permissions.cc',
      'common/extensions/permissions/chrome_api_permissions.h',
      'common/extensions/permissions/chrome_permission_message_provider.cc',
      'common/extensions/permissions/chrome_permission_message_provider.h',
      'common/extensions/permissions/chrome_permission_message_rules.cc',
      'common/extensions/permissions/chrome_permission_message_rules.h',
      'common/extensions/sync_helper.cc',
      'common/extensions/sync_helper.h',
      'common/extensions/webstore_install_result.cc',
      'common/extensions/webstore_install_result.h',
    ],
    'chrome_common_printing_sources': [
      'common/chrome_utility_printing_messages.h',
      'common/cloud_print/cloud_print_cdd_conversion.cc',
      'common/cloud_print/cloud_print_cdd_conversion.h',
      'common/cloud_print/cloud_print_class_mac.h',
      'common/cloud_print/cloud_print_class_mac.mm',
      'common/cloud_print/cloud_print_constants.cc',
      'common/cloud_print/cloud_print_constants.h',
      'common/cloud_print/cloud_print_helpers.cc',
      'common/cloud_print/cloud_print_helpers.h',
      'common/cloud_print/cloud_print_proxy_info.cc',
      'common/cloud_print/cloud_print_proxy_info.h',
    ],
    'chrome_common_extensions_chromeos_sources': [
      'common/extensions/api/file_browser_handlers/file_browser_handler.cc',
      'common/extensions/api/file_browser_handlers/file_browser_handler.h',
      'common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.cc',
      'common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h',
      'common/extensions/api/input_ime/input_components_handler.cc',
      'common/extensions/api/input_ime/input_components_handler.h',
    ],
    'chrome_common_shared_safe_browsing_sources': [
      'common/safe_browsing/file_type_policies.cc',
      'common/safe_browsing/file_type_policies.h',
    ],
    'chrome_common_full_safe_browsing_sources': [
      'common/safe_browsing/binary_feature_extractor.cc',
      'common/safe_browsing/binary_feature_extractor.h',
      'common/safe_browsing/binary_feature_extractor_mac.cc',
      'common/safe_browsing/binary_feature_extractor_posix.cc',
      'common/safe_browsing/binary_feature_extractor_win.cc',
      'common/safe_browsing/download_protection_util.cc',
      'common/safe_browsing/download_protection_util.h',
      'common/safe_browsing/ipc_protobuf_message_macros.h',
      'common/safe_browsing/ipc_protobuf_message_null_macros.h',
      'common/safe_browsing/mach_o_image_reader_mac.cc',
      'common/safe_browsing/mach_o_image_reader_mac.h',
      'common/safe_browsing/pe_image_reader_win.cc',
      'common/safe_browsing/pe_image_reader_win.h',
      'common/safe_browsing/protobuf_message_log_macros.h',
      'common/safe_browsing/protobuf_message_param_traits.h',
      'common/safe_browsing/protobuf_message_read_macros.h',
      'common/safe_browsing/protobuf_message_size_macros.h',
      'common/safe_browsing/protobuf_message_write_macros.h',
      'common/safe_browsing/zip_analyzer.cc',
      'common/safe_browsing/zip_analyzer.h',
      'common/safe_browsing/zip_analyzer_results.cc',
      'common/safe_browsing/zip_analyzer_results.h',
    ],
    'chrome_common_importer_sources': [
      'common/importer/edge_importer_utils_win.cc',
      'common/importer/edge_importer_utils_win.h',
      'common/importer/firefox_importer_utils.cc',
      'common/importer/firefox_importer_utils.h',
      'common/importer/firefox_importer_utils_linux.cc',
      'common/importer/firefox_importer_utils_mac.mm',
      'common/importer/firefox_importer_utils_win.cc',
      'common/importer/ie_importer_utils_win.cc',
      'common/importer/ie_importer_utils_win.h',
      'common/importer/imported_bookmark_entry.cc',
      'common/importer/imported_bookmark_entry.h',
      'common/importer/importer_autofill_form_data_entry.cc',
      'common/importer/importer_autofill_form_data_entry.h',
      'common/importer/importer_bridge.cc',
      'common/importer/importer_bridge.h',
      'common/importer/importer_data_types.cc',
      'common/importer/importer_data_types.h',
      'common/importer/importer_test_registry_overrider_win.cc',
      'common/importer/importer_test_registry_overrider_win.h',
      'common/importer/importer_type.h',
      'common/importer/importer_url_row.cc',
      'common/importer/importer_url_row.h',
      'common/importer/profile_import_process_messages.cc',
      'common/importer/profile_import_process_messages.h',
      'common/importer/profile_import_process_param_traits_macros.h',
      'common/importer/safari_importer_utils.h',
      'common/importer/safari_importer_utils.mm',
    ],
    'chrome_common_service_process_sources': [
      'common/service_messages.h',
      'common/service_process_util.cc',
      'common/service_process_util.h',
      'common/service_process_util_linux.cc',
      'common/service_process_util_mac.mm',
      'common/service_process_util_posix.cc',
      'common/service_process_util_posix.h',
      'common/service_process_util_win.cc',
    ],
    'chrome_common_win_mac_sources': [
      'common/media_galleries/itunes_library.cc',
      'common/media_galleries/itunes_library.h',
      'common/media_galleries/picasa_types.cc',
      'common/media_galleries/picasa_types.h',
      'common/media_galleries/pmp_constants.h',
    ],
    'chrome_common_networking_private_sources' : [
      'common/extensions/api/networking_private/networking_private_crypto.cc',
      'common/extensions/api/networking_private/networking_private_crypto.h',
    ],
  },
  'targets': [
    {
      # GN: //chrome/common:common
      'target_name': 'common',
      'type': 'static_library',
      'hard_dependency': 1,  # Because of transitive dep on version_header.
      'variables': {
        'chrome_common_target': 1,
        'enable_wexit_time_destructors': 1,
      },
      'include_dirs': [
          '..',
          '<(SHARED_INTERMEDIATE_DIR)',  # Needed by chrome_content_client.cc.
        ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'dependencies': [
        # TODO(gregoryd): chrome_resources and chrome_strings could be
        #  shared with the 64-bit target, but it does not work due to a gyp
        # issue.
        'installer_util',
        'safe_browsing_proto',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/base/base.gyp:base_static',
        '<(DEPTH)/chrome/chrome_features.gyp:chrome_common_features',
        '<(DEPTH)/chrome/chrome_resources.gyp:chrome_resources',
        '<(DEPTH)/chrome/chrome_resources.gyp:chrome_strings',
        '<(DEPTH)/chrome/chrome_resources.gyp:theme_resources',
        '<(DEPTH)/chrome/common_constants.gyp:common_constants',
        '<(DEPTH)/chrome/common/variations/fieldtrial_testing_config.gyp:fieldtrial_testing_config',
        '<(DEPTH)/components/components.gyp:cast_certificate',
        '<(DEPTH)/components/components.gyp:cloud_devices_common',
        '<(DEPTH)/components/components.gyp:component_updater',
        '<(DEPTH)/components/components.gyp:content_settings_core_common',
        '<(DEPTH)/components/components.gyp:crash_core_common',
        '<(DEPTH)/components/components.gyp:error_page_common',
        '<(DEPTH)/components/components.gyp:favicon_base',
        '<(DEPTH)/components/components.gyp:flags_ui_switches',
        '<(DEPTH)/components/components.gyp:gcm_driver_common',
        '<(DEPTH)/components/components.gyp:json_schema',
        '<(DEPTH)/components/components.gyp:metrics',
        '<(DEPTH)/components/components.gyp:metrics_net',
        '<(DEPTH)/components/components.gyp:network_session_configurator_switches',
        '<(DEPTH)/components/components.gyp:omnibox_common',
        '<(DEPTH)/components/components.gyp:policy',
        '<(DEPTH)/components/components.gyp:policy_component_common',
        '<(DEPTH)/components/components.gyp:translate_core_common',
        '<(DEPTH)/components/components.gyp:variations',
        '<(DEPTH)/components/components.gyp:version_info',
        '<(DEPTH)/components/components_strings.gyp:components_strings',
        '<(DEPTH)/components/prefs/prefs.gyp:prefs',
        '<(DEPTH)/components/url_formatter/url_formatter.gyp:url_formatter',
        '<(DEPTH)/content/content.gyp:content_common',
        '<(DEPTH)/crypto/crypto.gyp:crypto',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/skia/skia.gyp:skia_library',
        '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
        '<(DEPTH)/third_party/kasko/kasko.gyp:kasko_features',
        '<(DEPTH)/third_party/zlib/google/zip.gyp:zip',
        '<(DEPTH)/ui/gfx/ipc/geometry/gfx_ipc_geometry.gyp:gfx_ipc_geometry',
        '<(DEPTH)/ui/gfx/ipc/gfx_ipc.gyp:gfx_ipc',
        '<(DEPTH)/ui/gfx/ipc/skia/gfx_ipc_skia.gyp:gfx_ipc_skia',
        '<(DEPTH)/ui/resources/ui_resources.gyp:ui_resources',
        '<(DEPTH)/url/url.gyp:url_lib',
        '<(DEPTH)/url/ipc/url_ipc.gyp:url_ipc',
      ],
      'sources': [
        '<@(chrome_common_sources)'
      ],
      'conditions': [
        ['enable_extensions==1', {
          'sources': [ '<@(chrome_common_extensions_sources)' ],
          'dependencies': [
            '<(DEPTH)/device/usb/usb.gyp:device_usb',
            '<(DEPTH)/chrome/common/extensions/api/api.gyp:chrome_api',
            '<(DEPTH)/extensions/common/api/api.gyp:extensions_api',
            '<(DEPTH)/extensions/extensions.gyp:extensions_common',
            '<(DEPTH)/extensions/extensions_resources.gyp:extensions_resources',
            '<(DEPTH)/extensions/extensions_strings.gyp:extensions_strings',
            '<(DEPTH)/media/cast/cast.gyp:cast_net',
          ],
          'export_dependent_settings': [
            '<(DEPTH)/chrome/common/extensions/api/api.gyp:chrome_api',
          ],
          'conditions': [
            ['chromeos==1', {
              'sources': [ '<@(chrome_common_extensions_chromeos_sources)' ],
            }],
            ['OS=="win" or OS=="linux"', {
              'sources': [
                'common/extensions/api/input_ime/input_components_handler.cc',
                'common/extensions/api/input_ime/input_components_handler.h',
              ]
            }]
          ],
        }],
        ['enable_extensions==1 and chromeos==1', {
          'sources': [ '<@(chrome_common_extensions_chromeos_sources)' ],
        }],
        ['OS=="win" or OS=="mac"', {
          'sources': [ '<@(chrome_common_win_mac_sources)' ],
        }],
        ['OS=="win" or OS=="mac" or chromeos==1', {
          'sources': [ '<@(chrome_common_networking_private_sources)' ],
          'dependencies': [
            '../third_party/boringssl/boringssl.gyp:boringssl',
          ],
        }],
        ['OS != "ios"', {
          'dependencies': [
            'common_mojo_bindings',
            '<(DEPTH)/components/components.gyp:autofill_core_common',
            '<(DEPTH)/components/components.gyp:autofill_content_common',
            '<(DEPTH)/components/components.gyp:password_manager_content_mojo_bindings',
            '<(DEPTH)/components/components.gyp:password_manager_core_common',
            '<(DEPTH)/components/components.gyp:signin_core_common',
            '<(DEPTH)/components/components.gyp:translate_content_common',
            '<(DEPTH)/components/components.gyp:visitedlink_common',
            '<(DEPTH)/extensions/extensions.gyp:extensions_common_constants',
            '<(DEPTH)/ipc/ipc.gyp:ipc',
            '<(DEPTH)/media/media.gyp:media',
            '<(DEPTH)/third_party/re2/re2.gyp:re2',
            '<(DEPTH)/third_party/widevine/cdm/widevine_cdm.gyp:widevine_cdm_version_h',
          ],
        }, {  # OS == ios
          'sources/': [
            ['exclude', '^common/child_process_'],
            ['exclude', '^common/chrome_content_client\\.cc$'],
            ['exclude', '^common/chrome_version_info_posix\\.cc$'],
            ['exclude', '^common/common_message_generator\\.cc$'],
            ['exclude', '^common/common_param_traits'],
            ['exclude', '^common/custom_handlers/'],
            ['exclude', '^common/extensions/'],
            ['exclude', '^common/logging_chrome\\.'],
            ['exclude', '^common/media/media_resource_provider'],
            ['exclude', '^common/media_galleries/'],
            ['exclude', '^common/multi_process_'],
            ['exclude', '^common/profiling\\.'],
            ['exclude', '^common/resource_usage_reporter_type_converters'],
            ['exclude', '^common/spellcheck_'],
            ['exclude', '^common/validation_message_'],
            ['exclude', '^common/web_apps\\.'],
            # TODO(ios): Include files here as they are made to work; once
            # everything is online, remove everything below here and just
            # use the exclusions above.
            ['exclude', '\\.(cc|mm)$'],
            ['include', '_ios\\.(cc|mm)$'],
            ['include', '(^|/)ios/'],
            ['include', '^common/chrome_version_info\\.cc$'],
            ['include', '^common/translate'],
            ['include', '^common/zip'],
          ],
        }],
        ['disable_nacl==0', {
          'dependencies': [
            '<(DEPTH)/components/nacl.gyp:nacl_common',
          ],
        }],
        ['enable_plugins==1', {
          'dependencies': [
            '<(DEPTH)/third_party/adobe/flash/flash_player.gyp:flapper_version_h',
          ],
          'sources': [
            'common/pepper_flash.cc',
            'common/pepper_flash.h',
            'common/ppapi_utils.cc',
            'common/ppapi_utils.h',
          ],
        }],
        ['enable_plugins==1 and enable_extensions==1', {
          'sources': [
            'common/pepper_permission_util.cc',
            'common/pepper_permission_util.h',
          ],
        }],
        ['enable_basic_printing==1 or enable_print_preview==1', {
          'sources': [ '<@(chrome_common_printing_sources)' ],

          'dependencies': [
            '<(DEPTH)/components/components.gyp:printing_common',
            '<(DEPTH)/printing/printing.gyp:printing',
          ],
        }],
        ['enable_print_preview==1', {
          'sources': [ '<@(chrome_common_service_process_sources)' ],
        }],
        ['OS=="android"', {
          'sources!': [
            'common/channel_info_posix.cc',
            'common/extensions/api/spellcheck/spellcheck_handler.cc',
            'common/extensions/manifest_handlers/extension_action_handler.cc',
            'common/extensions/manifest_handlers/minimum_chrome_version_checker.cc',
            'common/media_galleries/metadata_types.h',
          ],
        }, {
          # Non-Android.
          'sources': [ '<@(chrome_common_importer_sources)' ],
          'dependencies': [
            '<(DEPTH)/url/ipc/url_ipc.gyp:url_ipc',
        ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            '<(DEPTH)/components/components.gyp:dom_distiller_core',  # Needed by chrome_content_client.cc.
          ],
          'all_dependent_settings': {
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalDependencies': [
                  'wintrust.lib',
                ],
              },
            },
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalDependencies': [
                'wintrust.lib',
              ],
            },
          },
        }],
        ['OS=="mac"', {
          'dependencies': [
            '../third_party/google_toolbox_for_mac/google_toolbox_for_mac.gyp:google_toolbox_for_mac',
            '../third_party/mach_override/mach_override.gyp:mach_override',
          ],
          'sources!': [
            'common/channel_info_posix.cc',
          ],
        }],
        ['enable_webrtc==0', {
          'sources!': [
            'common/media/webrtc_logging_messages.h',
          ]
        }],
        # Desktop uses full safe-browsing, including download protection
        ['safe_browsing==1', {
          'sources': [
            '<@(chrome_common_shared_safe_browsing_sources)',
            '<@(chrome_common_full_safe_browsing_sources)',
          ],
        }],
        # Android uses only the file_type_policies code.
        ['safe_browsing==2', {
          'sources': ['<@(chrome_common_shared_safe_browsing_sources)', ],
        }],
      ],
      'target_conditions': [
        ['OS == "ios"', {
          'sources/': [
            # Pull in specific Mac files for iOS (which have been filtered out
            # by file name rules).
            ['include', '^common/chrome_version_info_mac\\.mm$'],
          ],
        }],
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
      ],
    },
    {
      # GN version: //chrome/common/net
      'target_name': 'common_net',
      'type': 'static_library',
      'sources': [
        'common/net/net_resource_provider.cc',
        'common/net/net_resource_provider.h',
        'common/net/x509_certificate_model.cc',
        'common/net/x509_certificate_model.h',
        'common/net/x509_certificate_model_nss.cc',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/chrome/chrome_features.gyp:chrome_common_features',
        '<(DEPTH)/chrome/chrome_resources.gyp:chrome_resources',
        '<(DEPTH)/chrome/chrome_resources.gyp:chrome_strings',
        '<(DEPTH)/components/url_formatter/url_formatter.gyp:url_formatter',
        '<(DEPTH)/crypto/crypto.gyp:crypto',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/net/net.gyp:net_resources',
        '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
        '<(DEPTH)/ui/base/ui_base.gyp:ui_base',
      ],
      'conditions': [
        ['OS == "ios"', {
          'sources!': [
            'common/net/net_resource_provider.cc',
          ],
        }],
        ['OS == "android" or OS == "ios"', {
          'sources!': [
            'common/net/x509_certificate_model.cc',
          ],
        }],
        ['use_nss_certs == 1', {
          'sources': [
            # GN version: //chrome/third_party/mozilla_security_manager
            'third_party/mozilla_security_manager/nsNSSCertHelper.cpp',
            'third_party/mozilla_security_manager/nsNSSCertHelper.h',
            'third_party/mozilla_security_manager/nsNSSCertificate.cpp',
            'third_party/mozilla_security_manager/nsNSSCertificate.h',
            'third_party/mozilla_security_manager/nsUsageArrayHelper.cpp',
            'third_party/mozilla_security_manager/nsUsageArrayHelper.h',
          ],
          'dependencies': [
            '../build/linux/system.gyp:nss',
          ],
        }, {
          'sources!': [
            'common/net/x509_certificate_model_nss.cc',
          ],
        }],
        ['OS=="win"', {
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        }],
      ],
    },
    {
      # Protobuf compiler / generator for the safebrowsing client
      # model proto, the client-side detection (csd) request
      # proto, and the download file types proto.

      # GN version: //chrome/common/safe_browsing:proto
      'target_name': 'safe_browsing_proto',
      'type': 'static_library',
      'sources': [
        'common/safe_browsing/client_model.proto',
        'common/safe_browsing/crx_info.proto',
        'common/safe_browsing/csd.proto',
        'common/safe_browsing/download_file_types.proto',
        'common/safe_browsing/permission_report.proto',
      ],
      'variables': {
        'proto_in_dir': 'common/safe_browsing',
        'proto_out_dir': 'chrome/common/safe_browsing',
      },
      'includes': [ '../build/protoc.gypi' ],
    },
    {
      'target_name': 'common_mojo_bindings_mojom',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'common/image_decoder.mojom',
          'common/resource_usage_reporter.mojom',
        ],
        'mojom_typemaps': [
          '../skia/public/interfaces/skbitmap.typemap',
        ],
      },
      'dependencies': [
        '../mojo/mojo_public.gyp:mojo_cpp_bindings',
        '../skia/skia.gyp:skia_mojo',
      ],
      'includes': [ '../mojo/mojom_bindings_generator_explicit.gypi' ],
    },
    {
      # GN version: //chrome/common:mojo_bindings
      'target_name': 'common_mojo_bindings',
      'type': 'static_library',
      'dependencies': [
        'common_mojo_bindings_mojom',
        '../mojo/mojo_public.gyp:mojo_cpp_bindings',
        '../skia/skia.gyp:skia',
      ],
      'export_dependent_settings': [
        '../skia/skia.gyp:skia',
      ],
    },
  ],
  'conditions': [
    ['OS == "win"', {
      'targets': [
        {
          # GN version: //chrome/common:metrics_constants_util_win
          'target_name': 'metrics_constants_util_win',
          'type': 'static_library',
          'sources': [
            'common/metrics_constants_util_win.cc',
            'common/metrics_constants_util_win.h',
          ],
          'dependencies': [
            'installer_util',
            '../base/base.gyp:base',
          ],
        },
      ],
    }],
  ],
}
