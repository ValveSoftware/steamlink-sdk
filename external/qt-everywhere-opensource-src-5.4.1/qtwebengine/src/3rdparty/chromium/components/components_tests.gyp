# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # This turns on e.g. the filename-based detection of which
    # platforms to include source files on (e.g. files ending in
    # _mac.h or _mac.cc are only compiled on MacOSX).
    'chromium_code': 1,
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/components',
   },
  'conditions': [
    ['android_webview_build == 0', {
      'targets': [
        {
          'target_name': 'components_unittests',
          'type': '<(gtest_target_type)',
          'sources': [
            'auto_login_parser/auto_login_parser_unittest.cc',
            'autofill/content/browser/content_autofill_driver_unittest.cc',
            'autofill/content/browser/request_autocomplete_manager_unittest.cc',
            'autofill/content/browser/wallet/full_wallet_unittest.cc',
            'autofill/content/browser/wallet/instrument_unittest.cc',
            'autofill/content/browser/wallet/wallet_address_unittest.cc',
            'autofill/content/browser/wallet/wallet_client_unittest.cc',
            'autofill/content/browser/wallet/wallet_items_unittest.cc',
            'autofill/content/browser/wallet/wallet_service_url_unittest.cc',
            'autofill/content/browser/wallet/wallet_signin_helper_unittest.cc',
            'autofill/core/browser/address_field_unittest.cc',
            'autofill/core/browser/address_unittest.cc',
            'autofill/core/browser/android/auxiliary_profile_unittest_android.cc',
            'autofill/core/browser/autocomplete_history_manager_unittest.cc',
            'autofill/core/browser/autofill_country_unittest.cc',
            'autofill/core/browser/autofill_data_model_unittest.cc',
            'autofill/core/browser/autofill_download_unittest.cc',
            'autofill/core/browser/autofill_external_delegate_unittest.cc',
            'autofill/core/browser/autofill_field_unittest.cc',
            'autofill/core/browser/autofill_ie_toolbar_import_win_unittest.cc',
            'autofill/core/browser/autofill_manager_unittest.cc',
            'autofill/core/browser/autofill_merge_unittest.cc',
            'autofill/core/browser/autofill_metrics_unittest.cc',
            'autofill/core/browser/autofill_profile_unittest.cc',
            'autofill/core/browser/autofill_regexes_unittest.cc',
            'autofill/core/browser/autofill_type_unittest.cc',
            'autofill/core/browser/autofill_xml_parser_unittest.cc',
            'autofill/core/browser/contact_info_unittest.cc',
            'autofill/core/browser/credit_card_field_unittest.cc',
            'autofill/core/browser/credit_card_unittest.cc',
            'autofill/core/browser/form_field_unittest.cc',
            'autofill/core/browser/form_structure_unittest.cc',
            'autofill/core/browser/name_field_unittest.cc',
            'autofill/core/browser/password_generator_unittest.cc',
            'autofill/core/browser/personal_data_manager_unittest.cc',
            'autofill/core/browser/phone_field_unittest.cc',
            'autofill/core/browser/phone_number_i18n_unittest.cc',
            'autofill/core/browser/phone_number_unittest.cc',
            'autofill/core/browser/validation_unittest.cc',
            'autofill/core/browser/webdata/autofill_profile_syncable_service_unittest.cc',
            'autofill/core/browser/webdata/autofill_table_unittest.cc',
            'autofill/core/browser/webdata/web_data_service_unittest.cc',
            'autofill/core/common/form_data_unittest.cc',
            'autofill/core/common/form_field_data_unittest.cc',
            'autofill/core/common/password_form_fill_data_unittest.cc',
            'autofill/core/common/save_password_progress_logger_unittest.cc',
            'bookmarks/browser/bookmark_codec_unittest.cc',
            'bookmarks/browser/bookmark_expanded_state_tracker_unittest.cc',
            'bookmarks/browser/bookmark_index_unittest.cc',
            'bookmarks/browser/bookmark_model_unittest.cc',
            'bookmarks/browser/bookmark_utils_unittest.cc',
            'breakpad/app/crash_keys_win_unittest.cc',
            'captive_portal/captive_portal_detector_unittest.cc',
            'cloud_devices/common/cloud_devices_urls_unittest.cc',
            'cloud_devices/common/printer_description_unittest.cc',
            'data_reduction_proxy/browser/data_reduction_proxy_auth_request_handler_unittest.cc',
            'data_reduction_proxy/browser/data_reduction_proxy_config_service_unittest.cc',
            'data_reduction_proxy/browser/data_reduction_proxy_metrics_unittest.cc',
            'data_reduction_proxy/browser/data_reduction_proxy_params_unittest.cc',
            'data_reduction_proxy/browser/data_reduction_proxy_protocol_unittest.cc',
            'data_reduction_proxy/browser/data_reduction_proxy_settings_unittest.cc',
            'data_reduction_proxy/browser/http_auth_handler_data_reduction_proxy_unittest.cc',
            'data_reduction_proxy/common/data_reduction_proxy_headers_unittest.cc',
            'dom_distiller/core/article_entry_unittest.cc',
            'dom_distiller/core/distilled_content_store_unittest.cc',
            'dom_distiller/core/distiller_unittest.cc',
            'dom_distiller/core/distiller_url_fetcher_unittest.cc',
            'dom_distiller/core/dom_distiller_model_unittest.cc',
            'dom_distiller/core/dom_distiller_service_unittest.cc',
            'dom_distiller/core/dom_distiller_store_unittest.cc',
            'dom_distiller/core/task_tracker_unittest.cc',
            'dom_distiller/core/url_utils_unittest.cc',
            'dom_distiller/core/viewer_unittest.cc',
            'domain_reliability/config_unittest.cc',
            'domain_reliability/context_unittest.cc',
            'domain_reliability/dispatcher_unittest.cc',
            'domain_reliability/monitor_unittest.cc',
            'domain_reliability/scheduler_unittest.cc',
            'domain_reliability/test_util.cc',
            'domain_reliability/test_util.h',
            'domain_reliability/uploader_unittest.cc',
            'domain_reliability/util_unittest.cc',
            'enhanced_bookmarks/image_store_ios_unittest.mm',
            'enhanced_bookmarks/image_store_unittest.cc',
            'enhanced_bookmarks/metadata_accessor_unittest.cc',
            'feedback/feedback_common_unittest.cc',
            'feedback/feedback_data_unittest.cc',
            'feedback/feedback_uploader_unittest.cc',
            'gcm_driver/gcm_client_impl_unittest.cc',
            'gcm_driver/gcm_driver_desktop_unittest.cc',
            'gcm_driver/gcm_stats_recorder_impl_unittest.cc',
            'history/core/common/thumbnail_score_unittest.cc',
            'invalidation/invalidation_logger_unittest.cc',
            'json_schema/json_schema_validator_unittest.cc',
            'json_schema/json_schema_validator_unittest_base.cc',
            'json_schema/json_schema_validator_unittest_base.h',
            'keyed_service/content/browser_context_dependency_manager_unittest.cc',
            'keyed_service/core/dependency_graph_unittest.cc',
            'language_usage_metrics/language_usage_metrics_unittest.cc',
            'leveldb_proto/proto_database_impl_unittest.cc',
            'metrics/compression_utils_unittest.cc',
            'metrics/machine_id_provider_win_unittest.cc',
            'metrics/metrics_hashes_unittest.cc',
            'metrics/metrics_log_manager_unittest.cc',
            'metrics/metrics_log_unittest.cc',
            'metrics/metrics_reporting_scheduler_unittest.cc',
            'metrics/metrics_service_unittest.cc',
            'metrics/metrics_state_manager_unittest.cc',
            'metrics/persisted_logs_unittest.cc',
            'navigation_interception/intercept_navigation_resource_throttle_unittest.cc',
            'network_time/network_time_tracker_unittest.cc',
            'os_crypt/ie7_password_win_unittest.cc',
            'os_crypt/keychain_password_mac_unittest.mm',
            'os_crypt/os_crypt_unittest.cc',
            'password_manager/core/browser/browser_save_password_progress_logger_unittest.cc',
            'password_manager/core/browser/log_router_unittest.cc',
            'password_manager/core/browser/login_database_unittest.cc',
            'password_manager/core/browser/password_autofill_manager_unittest.cc',
            'password_manager/core/browser/password_form_manager_unittest.cc',
            'password_manager/core/browser/password_generation_manager_unittest.cc',
            'password_manager/core/browser/password_manager_unittest.cc',
            'password_manager/core/browser/password_store_default_unittest.cc',
            'password_manager/core/browser/password_store_unittest.cc',
            'password_manager/core/browser/password_syncable_service_unittest.cc',
            'password_manager/core/browser/psl_matching_helper_unittest.cc',
            'precache/content/precache_manager_unittest.cc',
            'precache/core/precache_database_unittest.cc',
            'precache/core/precache_fetcher_unittest.cc',
            'precache/core/precache_url_table_unittest.cc',
            'query_parser/query_parser_unittest.cc',
            'query_parser/snippet_unittest.cc',
            'rappor/bloom_filter_unittest.cc',
            'rappor/byte_vector_utils_unittest.cc',
            'rappor/log_uploader_unittest.cc',
            'rappor/rappor_metric_unittest.cc',
            'rappor/rappor_service_unittest.cc',
            'search_provider_logos/logo_cache_unittest.cc',
            'search_provider_logos/logo_tracker_unittest.cc',
            'sessions/serialized_navigation_entry_unittest.cc',
            'signin/core/browser/mutable_profile_oauth2_token_service_unittest.cc',
            'signin/core/browser/signin_error_controller_unittest.cc',
            'signin/core/browser/webdata/token_service_table_unittest.cc',
            'signin/ios/browser/profile_oauth2_token_service_ios_unittest.mm',
            'storage_monitor/image_capture_device_manager_unittest.mm',
            'storage_monitor/media_storage_util_unittest.cc',
            'storage_monitor/media_transfer_protocol_device_observer_linux_unittest.cc',
            'storage_monitor/storage_info_unittest.cc',
            'storage_monitor/storage_monitor_chromeos_unittest.cc',
            'storage_monitor/storage_monitor_linux_unittest.cc',
            'storage_monitor/storage_monitor_mac_unittest.mm',
            'storage_monitor/storage_monitor_unittest.cc',
            'storage_monitor/storage_monitor_win_unittest.cc',
            'sync_driver/non_ui_data_type_controller_unittest.cc',
            'sync_driver/data_type_manager_impl_unittest.cc',
            'sync_driver/generic_change_processor_unittest.cc',
            'sync_driver/model_association_manager_unittest.cc',
            'sync_driver/non_blocking_data_type_controller_unittest.cc',
            'sync_driver/shared_change_processor_unittest.cc',
            'sync_driver/sync_prefs_unittest.cc',
            'sync_driver/system_encryptor_unittest.cc',
            'sync_driver/ui_data_type_controller_unittest.cc',
            'test/run_all_unittests.cc',
            'translate/core/browser/language_state_unittest.cc',
            'translate/core/browser/translate_browser_metrics_unittest.cc',
            'translate/core/browser/translate_prefs_unittest.cc',
            'translate/core/browser/translate_script_unittest.cc',
            'translate/core/common/translate_metrics_unittest.cc',
            'translate/core/common/translate_util_unittest.cc',
            'translate/core/language_detection/language_detection_util_unittest.cc',
            'url_matcher/regex_set_matcher_unittest.cc',
            'url_matcher/string_pattern_unittest.cc',
            'url_matcher/substring_set_matcher_unittest.cc',
            'url_matcher/url_matcher_factory_unittest.cc',
            'url_matcher/url_matcher_unittest.cc',
            'url_fixer/url_fixer_unittest.cc',
            'variations/active_field_trials_unittest.cc',
            'variations/caching_permuted_entropy_provider_unittest.cc',
            'variations/entropy_provider_unittest.cc',
            'variations/metrics_util_unittest.cc',
            'variations/study_filtering_unittest.cc',
            'variations/variations_associated_data_unittest.cc',
            'variations/variations_seed_processor_unittest.cc',
            'variations/variations_seed_simulator_unittest.cc',
            'visitedlink/test/visitedlink_unittest.cc',
            'web_modal/web_contents_modal_dialog_manager_unittest.cc',
          ],
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            '../base/base.gyp:base_prefs_test_support',
            '../base/base.gyp:test_support_base',
            # TODO(blundell): Eliminate this dependency by having
            # components_unittests have its own pakfile. crbug.com/348563
            '../chrome/chrome_resources.gyp:packed_extra_resources',
            # TODO(blundell): Eliminate the need for this dependency in code
            # that iOS shares. crbug.com/325243
            '../content/content_shell_and_tests.gyp:test_support_content',
            '../sync/sync.gyp:sync',
            '../sync/sync.gyp:test_support_sync_api',
            '../testing/gmock.gyp:gmock',
            '../testing/gtest.gyp:gtest',
            '../ui/base/ui_base.gyp:ui_base',
            '../ui/gfx/gfx.gyp:gfx',

            'components_resources.gyp:components_resources',

            # Dependencies of auto_login_parser
            'components.gyp:auto_login_parser',

            # Dependencies of autofill
            'components.gyp:autofill_core_browser',
            'components.gyp:autofill_core_common',
            'components.gyp:autofill_core_test_support',
            'components_strings.gyp:components_strings',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber',

            # Dependencies of breakpad
            'components.gyp:breakpad_test_support',

            # Dependencies of bookmarks
            'components.gyp:bookmarks_browser',
            'components.gyp:bookmarks_test_support',

            # Dependencies of captive_portal
            'components.gyp:captive_portal_test_support',
            '../net/net.gyp:net_test_support',

            # Dependencies of cloud_devices
            'components.gyp:cloud_devices_common',

            # Dependencies of data_reduction_proxy
            'components.gyp:data_reduction_proxy_browser',
            'components.gyp:data_reduction_proxy_common',
            'components.gyp:data_reduction_proxy_test_support',

            # Dependencies of dom_distiller
            'components.gyp:distilled_page_proto',
            'components.gyp:dom_distiller_core',
            'components.gyp:dom_distiller_test_support',

            # Dependencies of domain_reliability
            'components.gyp:domain_reliability',

            # Dependencies of enhanced_bookmarks
            'components.gyp:enhanced_bookmarks',
            'components.gyp:enhanced_bookmarks_test_support',

            # Dependencies of feedback
            'components.gyp:feedback_component',

            # Dependencies of gcm
            'components.gyp:gcm_driver',
            'components.gyp:gcm_driver_test_support',

            # Dependencies of history
            'components.gyp:history_core_common',

            # Dependencies of invalidation
            'components.gyp:invalidation',
            'components.gyp:invalidation_test_support',

            # Dependencies of json_schema
            'components.gyp:json_schema',

            # Dependencies of keyed_service
            'components.gyp:keyed_service_core',

            # Dependencies of language_usage_metrics
            'components.gyp:language_usage_metrics',

            # Dependencies of leveldb_proto
            '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
            'components.gyp:leveldb_proto',
            'components.gyp:leveldb_proto_test_support',

            # Dependencies of metrics
            'components.gyp:metrics',
            'components.gyp:metrics_net',
            'components.gyp:metrics_test_support',

            # Dependencies of network_time
            'components.gyp:network_time',

            # Dependencies of os_crypt
            'components.gyp:os_crypt',

            # Dependencies of password_manager
            'components.gyp:password_manager_core_browser',
            'components.gyp:password_manager_core_browser_test_support',

            # Dependencies of precache/core
            'components.gyp:password_manager_core_browser',
            'components.gyp:precache_core',

            # Dependencies of pref_registry
            'components.gyp:pref_registry_test_support',

            # Dependencies of query_parser
            'components.gyp:query_parser',

            # Dependencies of rappor
            'components.gyp:rappor',

            # Dependencies of search_provider_logos
            'components.gyp:search_provider_logos',

            # Dependencies of signin
            'components.gyp:signin_core_browser',
            'components.gyp:signin_core_browser_test_support',
            '../google_apis/google_apis.gyp:google_apis_test_support',

            # Dependencies of sync_driver
            'components.gyp:sync_driver_test_support',

            # Dependencies of translate.
            'components.gyp:translate_core_browser',
            'components.gyp:translate_core_common',
            'components.gyp:translate_core_language_detection',

            # Dependencies of url_fixer
            'components.gyp:url_fixer',
            '../url/url.gyp:url_lib',

            # Dependencies of variations
            'components.gyp:variations',
          ],
          'conditions': [
            ['toolkit_views == 1', {
              'sources': [
                'bookmarks/browser/bookmark_node_data_unittest.cc',
              ],
            }],
            ['OS != "ios"', {
              'sources': [
                'autofill/content/renderer/renderer_save_password_progress_logger_unittest.cc',
                'dom_distiller/content/dom_distiller_viewer_source_unittest.cc',
                'usb_service/usb_context_unittest.cc',
              ],
              'dependencies': [
                # Dependencies of autofill
                'components.gyp:autofill_content_browser',
                'components.gyp:autofill_content_renderer',
                'components.gyp:autofill_content_test_support',

                # Dependencies of dom_distiller
                'components.gyp:dom_distiller_content',

                # Dependencies of
                # intercept_navigation_resource_throttle_unittest.cc
                '../skia/skia.gyp:skia',
                'components.gyp:navigation_interception',

                # Dependencies of keyed_service
                'components.gyp:keyed_service_content',

                # Dependencies of precache/content
                'components.gyp:precache_content',

                # Dependencies of sessions
                '../third_party/protobuf/protobuf.gyp:protobuf_lite',
                'components.gyp:sessions',
                'components.gyp:sessions_test_support',

                # Dependencies of storage monitor
                'components.gyp:storage_monitor',
                'components.gyp:storage_monitor_test_support',

                # Dependencies of url_matcher.
                'components.gyp:url_matcher',

                # Dependencies of usb_service tests.
                'components.gyp:usb_service',
                '../third_party/libusb/libusb.gyp:libusb',

                # Dependencies of visitedlink
                'components.gyp:visitedlink_browser',
                'components.gyp:visitedlink_renderer',
                '../content/content_resources.gyp:content_resources',

                # Dependencies of web_modal
                'components.gyp:web_modal',
                'components.gyp:web_modal_test_support',
              ],
            }, { # 'OS == "ios"'
              'includes': ['../chrome/chrome_ios_bundle_resources.gypi'],
              'sources/': [
                ['exclude', '\\.cc$'],
                ['exclude', '\\.mm$'],
                ['include', '^test/run_all_unittests\\.cc$'],
                ['include', '^auto_login_parser/'],
                ['include', '^autofill/core/'],
                ['include', '^bookmarks/'],
                ['include', '^data_reduction_proxy/'],
                ['include', '^dom_distiller/'],
                ['include', '^enhanced_bookmarks/'],
                ['include', '^gcm_driver/'],
                ['include', '^history/'],
                ['include', '^invalidation/'],
                ['include', '^json_schema/'],
                ['include', '^keyed_service/core/'],
                ['include', '^language_usage_metrics/'],
                ['include', '^leveldb_proto/'],
                ['include', '^metrics/'],
                ['include', '^network_time/'],
                ['include', '^password_manager/'],
                ['include', '^precache/core/'],
                ['include', '^search_provider_logos/'],
                ['include', '^signin/'],
                ['include', '^sync_driver/'],
                ['include', '^translate/'],
                ['include', '^url_fixer/'],
                ['include', '^variations/'],
              ],
              'dependencies': [
                # Dependencies of signin
                'components.gyp:signin_ios_browser',
                '../ios/ios_tests.gyp:test_support_ios',
              ],
              'actions': [
                {
                  'action_name': 'copy_test_data',
                  'variables': {
                    'test_data_files': [
                      'test/data',
                    ],
                    'test_data_prefix': 'components',
                  },
                  'includes': [ '../build/copy_test_data_ios.gypi' ],
                },
              ],
              'conditions': [
                ['configuration_policy==1', {
                  'sources/': [
                    ['include', '^policy/'],
                  ],
                }],
              ],
            }],
            ['disable_nacl==0', {
              'sources': [
                'nacl/browser/nacl_file_host_unittest.cc',
                'nacl/browser/nacl_process_host_unittest.cc',
                'nacl/browser/nacl_validation_cache_unittest.cc',
                'nacl/browser/pnacl_host_unittest.cc',
                'nacl/browser/pnacl_translation_cache_unittest.cc',
                'nacl/browser/test_nacl_browser_delegate.cc',
                'nacl/zygote/nacl_fork_delegate_linux_unittest.cc',
              ],
              'dependencies': [
                'nacl.gyp:nacl_browser',
                'nacl.gyp:nacl_common',
              ],
            }],
            ['OS == "mac"', {
              'link_settings': {
                'libraries': [
                  '$(SDKROOT)/System/Library/Frameworks/AddressBook.framework',
                  '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
                  '$(SDKROOT)/System/Library/Frameworks/ImageCaptureCore.framework',
                ],
              },
              'sources!': [
                'password_manager/core/browser/password_store_default_unittest.cc',
              ],
            }],
            ['OS == "android"', {
              'sources!': [
                'gcm_driver/gcm_client_impl_unittest.cc',
                'gcm_driver/gcm_driver_desktop_unittest.cc',
                'feedback/feedback_common_unittest.cc',
                'feedback/feedback_data_unittest.cc',
                'feedback/feedback_uploader_unittest.cc',
                'gcm_driver/gcm_stats_recorder_impl_unittest.cc',
                'signin/core/browser/mutable_profile_oauth2_token_service_unittest.cc',
                'storage_monitor/media_storage_util_unittest.cc',
                'storage_monitor/storage_info_unittest.cc',
                'storage_monitor/storage_monitor_unittest.cc',
                'usb_service/usb_context_unittest.cc',
                'web_modal/web_contents_modal_dialog_manager_unittest.cc',
              ],
              'dependencies': [
                '../testing/android/native_test.gyp:native_test_native_code',
              ],
              'dependencies!': [
                'components.gyp:feedback_component',
                'components.gyp:storage_monitor',
                'components.gyp:storage_monitor_test_support',
                'components.gyp:usb_service',
                'components.gyp:web_modal',
                'components.gyp:web_modal_test_support',
                '../third_party/libusb/libusb.gyp:libusb',
              ],
            }],
            ['OS != "android"', {
              'sources': [
                'invalidation/fake_invalidator_unittest.cc',
                'invalidation/gcm_network_channel_unittest.cc',
                'invalidation/invalidation_notifier_unittest.cc',
                'invalidation/invalidator_registrar_unittest.cc',
                'invalidation/non_blocking_invalidator_unittest.cc',
                'invalidation/p2p_invalidator_unittest.cc',
                'invalidation/push_client_channel_unittest.cc',
                'invalidation/sync_invalidation_listener_unittest.cc',
                'invalidation/sync_system_resources_unittest.cc',
                'invalidation/ticl_invalidation_service_unittest.cc',
              ],
            }],
            ['chromeos==1', {
              'sources': [
                'metrics/chromeos/serialization_utils_unittest.cc',
              ],
              'sources!': [
                'storage_monitor/storage_monitor_linux_unittest.cc',
              ],
              'dependencies': [
                '../chromeos/chromeos.gyp:chromeos_test_support',
                'components.gyp:metrics_chromeos',
              ],
            }],
            ['OS=="linux"', {
              'dependencies': [
                '../dbus/dbus.gyp:dbus',
                '../device/media_transfer_protocol/media_transfer_protocol.gyp:device_media_transfer_protocol',
              ],
            }],
            ['OS=="linux" and use_udev==0', {
              'dependencies!': [
                '../third_party/libusb/libusb.gyp:libusb',
                'components.gyp:storage_monitor',
                'components.gyp:storage_monitor_test_support',
              ],
              'sources/': [
                ['exclude', '^storage_monitor/'],
                ['exclude', '^usb_service/'],
              ],
            }],
            ['OS=="win" and win_use_allocator_shim==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
            ['OS=="linux" and component=="shared_library" and use_allocator!="none"', {
            'dependencies': [
                '<(DEPTH)/base/allocator/allocator.gyp:allocator',
            ],
            'link_settings': {
                'ldflags': ['-rdynamic'],
            },
            }],
            ['configuration_policy==1', {
              'dependencies': [
                'components.gyp:policy_component',
                'components.gyp:policy_component_test_support',
                'components.gyp:policy_test_support',
              ],
              'sources': [
                'policy/core/browser/autofill_policy_handler_unittest.cc',
                'policy/core/browser/browser_policy_connector_unittest.cc',
                'policy/core/browser/configuration_policy_handler_unittest.cc',
                'policy/core/browser/configuration_policy_pref_store_unittest.cc',
                'policy/core/browser/managed_bookmarks_tracker_unittest.cc',
                'policy/core/browser/url_blacklist_policy_handler_unittest.cc',
                'policy/core/common/async_policy_provider_unittest.cc',
                'policy/core/common/cloud/cloud_policy_client_unittest.cc',
                'policy/core/common/cloud/cloud_policy_core_unittest.cc',
                'policy/core/common/cloud/cloud_policy_manager_unittest.cc',
                'policy/core/common/cloud/cloud_policy_refresh_scheduler_unittest.cc',
                'policy/core/common/cloud/cloud_policy_service_unittest.cc',
                'policy/core/common/cloud/cloud_policy_validator_unittest.cc',
                'policy/core/common/cloud/component_cloud_policy_service_unittest.cc',
                'policy/core/common/cloud/component_cloud_policy_store_unittest.cc',
                'policy/core/common/cloud/component_cloud_policy_updater_unittest.cc',
                'policy/core/common/cloud/device_management_service_unittest.cc',
                'policy/core/common/cloud/external_policy_data_fetcher_unittest.cc',
                'policy/core/common/cloud/external_policy_data_updater_unittest.cc',
                'policy/core/common/cloud/policy_header_io_helper_unittest.cc',
                'policy/core/common/cloud/policy_header_service_unittest.cc',
                'policy/core/common/cloud/rate_limiter_unittest.cc',
                'policy/core/common/cloud/resource_cache_unittest.cc',
                'policy/core/common/cloud/user_cloud_policy_manager_unittest.cc',
                'policy/core/common/cloud/user_cloud_policy_store_unittest.cc',
                'policy/core/common/cloud/user_info_fetcher_unittest.cc',
                'policy/core/common/config_dir_policy_loader_unittest.cc',
                'policy/core/common/forwarding_policy_provider_unittest.cc',
                'policy/core/common/generate_policy_source_unittest.cc',
                'policy/core/common/policy_bundle_unittest.cc',
                'policy/core/common/policy_loader_ios_unittest.mm',
                'policy/core/common/policy_loader_mac_unittest.cc',
                'policy/core/common/policy_loader_win_unittest.cc',
                'policy/core/common/policy_map_unittest.cc',
                'policy/core/common/policy_provider_android_unittest.cc',
                'policy/core/common/policy_service_impl_unittest.cc',
                'policy/core/common/policy_statistics_collector_unittest.cc',
                'policy/core/common/preg_parser_win_unittest.cc',
                'policy/core/common/registry_dict_win_unittest.cc',
                'policy/core/common/schema_map_unittest.cc',
                'policy/core/common/schema_registry_unittest.cc',
                'policy/core/common/schema_unittest.cc',
              ],
              'conditions': [
                ['OS=="android"', {
                  'sources/': [
                    ['exclude', '^policy/core/common/async_policy_provider_unittest\\.cc'],
                  ],
                }],
                ['OS=="android" or OS=="ios"', {
                  # Note: 'sources!' is processed before any 'sources/', so the
                  # ['include', '^policy/'] on iOS above will include all of the
                  # policy source files again. Using 'source/' here too will get
                  # these files excluded as expected.
                  'sources/': [
                    ['exclude', '^policy/core/common/cloud/component_cloud_policy_service_unittest\\.cc'],
                    ['exclude', '^policy/core/common/cloud/component_cloud_policy_store_unittest\\.cc'],
                    ['exclude', '^policy/core/common/cloud/component_cloud_policy_updater_unittest\\.cc'],
                    ['exclude', '^policy/core/common/cloud/external_policy_data_fetcher_unittest\\.cc'],
                    ['exclude', '^policy/core/common/cloud/external_policy_data_updater_unittest\\.cc'],
                    ['exclude', '^policy/core/common/cloud/resource_cache_unittest\\.cc'],
                    ['exclude', '^policy/core/common/config_dir_policy_loader_unittest\\.cc'],
                  ],
                }],
                ['chromeos==1', {
                  'sources': [
                    'policy/core/common/proxy_policy_provider_unittest.cc',
                  ],
                  'sources!': [
                    'policy/core/common/cloud/user_cloud_policy_manager_unittest.cc',
                    'policy/core/common/cloud/user_cloud_policy_store_unittest.cc',
                  ],
                }],
                ['OS=="ios" or OS=="mac"', {
                  'sources': [
                    'policy/core/common/mac_util_unittest.cc',
                  ],
                }],
              ],
            }],
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        },
      ],
    }],
    ['OS != "ios" and android_webview_build == 0', {
      'targets': [
        {
          'target_name': 'components_perftests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_perf',
            '../content/content_shell_and_tests.gyp:test_support_content',
            '../testing/gtest.gyp:gtest',
            'components.gyp:visitedlink_browser',
          ],
         'include_dirs': [
           '..',
         ],
         'sources': [
           'visitedlink/test/visitedlink_perftest.cc',
         ],
         'conditions': [
           ['OS == "android"', {
             'dependencies': [
               '../testing/android/native_test.gyp:native_test_native_code',
             ],
           }],
         ],
         # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
         'msvs_disabled_warnings': [ 4267, ],
        },
      ],
      'conditions': [
        ['OS == "android"', {
          'targets': [
            {
              'target_name': 'components_unittests_apk',
              'type': 'none',
              'dependencies': [
                'components_unittests',
              ],
              'variables': {
                'test_suite_name': 'components_unittests',
              },
              'includes': [ '../build/apk_test.gypi' ],
            },
          ],
        }],
      ],
    }],
    ['OS!="ios"', {
      'targets': [
        {
          'target_name': 'components_browsertests',
          'type': '<(gtest_target_type)',
          'defines!': ['CONTENT_IMPLEMENTATION'],
          'dependencies': [
            'components.gyp:autofill_content_browser',
            'components.gyp:dom_distiller_content',
            'components.gyp:dom_distiller_core',
            'components_resources.gyp:components_resources',
            '../content/content.gyp:content_common',
            '../content/content.gyp:content_gpu',
            '../content/content.gyp:content_plugin',
            '../content/content.gyp:content_renderer',
            '../content/content_resources.gyp:content_resources',
            '../content/content_shell_and_tests.gyp:content_browser_test_support',
            '../content/content_shell_and_tests.gyp:content_shell_lib',
            '../content/content_shell_and_tests.gyp:content_shell_pak',
            '../content/content_shell_and_tests.gyp:test_support_content',
            '../skia/skia.gyp:skia',
            '../testing/gmock.gyp:gmock',
            '../testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '..',
          ],
          'defines': [
            'HAS_OUT_OF_PROC_TEST_RUNNER',
          ],
          'sources': [
            'autofill/content/browser/risk/fingerprint_browsertest.cc',
            'dom_distiller/content/distiller_page_web_contents_browsertest.cc',
          ],
          'actions': [
            {
              'action_name': 'repack_components_pack',
              'variables': {
                'pak_inputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/components/component_resources.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/components/strings/components_strings_en-US.pak',
                ],
                'pak_output': '<(PRODUCT_DIR)/components_resources.pak',
              },
              'includes': [ '../build/repack_action.gypi' ],
            },
          ],
          'conditions': [
            ['OS == "android"', {
              'sources!': [
                'autofill/content/browser/risk/fingerprint_browsertest.cc',
              ],
            }],
            ['OS == "linux"', {
              'sources': [
                  # content_extractor is a standalone content extraction tool built as
                  # a MANUAL component_browsertest.
                  'dom_distiller/standalone/content_extractor.cc',
                ],
            }],
            ['OS=="win"', {
              'resource_include_dirs': [
                '<(SHARED_INTERMEDIATE_DIR)/webkit',
              ],
              'sources': [
                '../content/shell/app/resource.h',
                '../content/shell/app/shell.rc',
                # TODO:  It would be nice to have these pulled in
                # automatically from direct_dependent_settings in
                # their various targets (net.gyp:net_resources, etc.),
                # but that causes errors in other targets when
                # resulting .res files get referenced multiple times.
                '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/webkit/blink_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_strings_en-US.rc',
              ],
              'dependencies': [
                '<(DEPTH)/net/net.gyp:net_resources',
                '<(DEPTH)/third_party/iaccessible2/iaccessible2.gyp:iaccessible2',
                '<(DEPTH)/third_party/isimpledom/isimpledom.gyp:isimpledom',
                '<(DEPTH)/webkit/webkit_resources.gyp:webkit_strings',
                '<(DEPTH)/webkit/webkit_resources.gyp:webkit_resources',
              ],
              'configurations': {
                'Debug_Base': {
                  'msvs_settings': {
                    'VCLinkerTool': {
                      'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                    },
                  },
                },
              },
              # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
              'msvs_disabled_warnings': [ 4267, ],
            }],
            ['OS=="win" and win_use_allocator_shim==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        },
      ],
    }],
  ],
}
