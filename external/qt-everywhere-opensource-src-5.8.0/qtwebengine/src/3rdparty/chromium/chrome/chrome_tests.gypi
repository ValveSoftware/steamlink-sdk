# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'build_angle_deqp_tests%': 0,
    'chrome_browser_extensions_test_support_sources': [
      # A list of sources which is shared between different browser tests.
      'browser/apps/app_browsertest_util.cc',
      'browser/apps/app_browsertest_util.h',
      'browser/extensions/browsertest_util.cc',
      'browser/extensions/browsertest_util.h',
      'browser/extensions/extension_apitest.cc',
      'browser/extensions/extension_apitest.h',
      'browser/extensions/extension_browsertest.cc',
      'browser/extensions/extension_browsertest.h',
      'browser/extensions/extension_function_test_utils.cc',
      'browser/extensions/extension_function_test_utils.h',
      'browser/extensions/extension_test_notification_observer.cc',
      'browser/extensions/extension_test_notification_observer.h',
      'browser/extensions/updater/extension_cache_fake.cc',
      'browser/extensions/updater/extension_cache_fake.h',
    ],
    # TODO(rockot) bug 505926: These should be moved to extensions_browsertests
    # but have old dependencies on chrome files. The chrome dependencies should
    # be removed and these moved to the extensions_browsertests target.
    'chrome_browser_tests_extensions_sources': [
      '../extensions/browser/api/app_window/app_window_apitest.cc',
      '../extensions/browser/api/bluetooth/bluetooth_apitest.cc',
      '../extensions/browser/api/bluetooth/bluetooth_private_apitest.cc',
      '../extensions/browser/api/cast_channel/cast_channel_apitest.cc',
      '../extensions/browser/api/runtime/runtime_apitest.cc',
      '../extensions/browser/api/serial/serial_apitest.cc',
      '../extensions/browser/api/usb/usb_manual_apitest.cc',
      '../extensions/browser/app_window/app_window_browsertest.cc',
      '../extensions/browser/guest_view/extension_options/extension_options_apitest.cc',
      '../extensions/browser/guest_view/mime_handler_view/mime_handler_view_browsertest.cc',
      '../extensions/renderer/console_apitest.cc',
      '../extensions/renderer/script_context_browsertest.cc'
    ],
    'chrome_browser_tests_display_source_apitest': [
      '../extensions/browser/api/display_source/display_source_apitestbase.cc',
      '../extensions/browser/api/display_source/display_source_apitestbase.h',
      'browser/extensions/api/display_source/display_source_wifi_display_apitest.cc',
    ],
    'chrome_browser_tests_sources': [
      # The list of sources which is only used by chrome browser tests.
      '../apps/app_restore_service_browsertest.cc',
      '../apps/load_and_launch_browsertest.cc',
      'app/chrome_command_ids.h',
      'app/chrome_dll.rc',
      'app/chrome_dll_resource.h',
      'app/chrome_version.rc.version',
      'browser/accessibility/browser_accessibility_state_browsertest.cc',
      'browser/app_controller_mac_browsertest.mm',
      'browser/apps/app_browsertest.cc',
      'browser/apps/app_shim/app_shim_host_manager_browsertest_mac.mm',
      'browser/apps/app_shim/test/app_shim_host_manager_test_api_mac.cc',
      'browser/apps/app_shim/test/app_shim_host_manager_test_api_mac.h',
      'browser/apps/app_speech_recognition_browsertest.cc',
      'browser/apps/app_url_redirector_browsertest.cc',
      'browser/apps/app_window_browsertest.cc',
      'browser/apps/event_page_browsertest.cc',
      'browser/apps/guest_view/app_view_browsertest.cc',
      'browser/apps/guest_view/extension_view/extension_view_browsertest.cc',
      'browser/apps/guest_view/web_view_browsertest.cc',
      'browser/apps/service_worker_browsertest.cc',
      'browser/apps/window_controls_browsertest.cc',
      'browser/autocomplete/autocomplete_browsertest.cc',
      'browser/autofill/autofill_browsertest.cc',
      'browser/autofill/autofill_server_browsertest.cc',
      'browser/autofill/content_autofill_driver_browsertest.cc',
      'browser/autofill/form_structure_browsertest.cc',
      'browser/banners/app_banner_data_fetcher_browsertest.cc',
      'browser/bitmap_fetcher/bitmap_fetcher_browsertest.cc',
      'browser/browser_encoding_browsertest.cc',
      'browser/browsing_data/autofill_counter_browsertest.cc',
      'browser/browsing_data/browsing_data_cache_storage_helper_browsertest.cc',
      'browser/browsing_data/browsing_data_database_helper_browsertest.cc',
      'browser/browsing_data/browsing_data_helper_browsertest.h',
      'browser/browsing_data/browsing_data_indexed_db_helper_browsertest.cc',
      'browser/browsing_data/browsing_data_local_storage_helper_browsertest.cc',
      'browser/browsing_data/browsing_data_remover_browsertest.cc',
      'browser/browsing_data/browsing_data_remover_test_util.cc',
      'browser/browsing_data/browsing_data_remover_test_util.h',
      'browser/browsing_data/cache_counter_browsertest.cc',
      'browser/browsing_data/conditional_cache_deletion_helper_browsertest.cc',
      'browser/browsing_data/downloads_counter_browsertest.cc',
      'browser/browsing_data/history_counter_browsertest.cc',
      'browser/browsing_data/media_licenses_counter_browsertest.cc',
      'browser/browsing_data/passwords_counter_browsertest.cc',
      'browser/chrome_browser_main_mac_browsertest.mm',
      'browser/chrome_content_browser_client_browsertest.cc',
      'browser/chrome_main_browsertest.cc',
      'browser/chrome_navigation_browsertest.cc',
      'browser/chrome_plugin_browsertest.cc',
      'browser/chrome_security_exploit_browsertest.cc',
      'browser/chrome_service_worker_browsertest.cc',
      'browser/chrome_site_per_process_browsertest.cc',
      'browser/chrome_switches_browsertest.cc',
      'browser/collected_cookies_browsertest.cc',
      'browser/content_settings/content_settings_browsertest.cc',
      'browser/crash_recovery_browsertest.cc',
      'browser/custom_handlers/protocol_handler_registry_browsertest.cc',
      'browser/data_saver/data_saver_browsertest.cc',
      'browser/devtools/device/adb/adb_client_socket_browsertest.cc',
      'browser/devtools/device/adb/mock_adb_server.cc',
      'browser/devtools/device/adb/mock_adb_server.h',
      'browser/devtools/device/port_forwarding_browsertest.cc',
      'browser/devtools/device/usb/android_usb_browsertest.cc',
      'browser/devtools/devtools_sanity_browsertest.cc',
      'browser/devtools/devtools_window_testing.cc',
      'browser/devtools/devtools_window_testing.h',
      'browser/do_not_track_browsertest.cc',
      'browser/dom_distiller/distillable_page_utils_browsertest.cc',
      'browser/dom_distiller/dom_distiller_viewer_source_browsertest.cc',
      'browser/dom_distiller/tab_utils_browsertest.cc',
      'browser/download/download_browsertest.cc',
      'browser/download/download_browsertest.h',
      'browser/download/download_danger_prompt_browsertest.cc',
      'browser/download/download_started_animation_browsertest.cc',
      'browser/download/save_page_browsertest.cc',
      'browser/errorpage_browsertest.cc',
      'browser/extensions/active_tab_apitest.cc',
      'browser/extensions/activity_log/activity_log_browsertest.cc',
      'browser/extensions/alert_apitest.cc',
      'browser/extensions/all_urls_apitest.cc',
      'browser/extensions/api/activity_log_private/activity_log_private_apitest.cc',
      'browser/extensions/api/autofill_private/autofill_private_apitest.cc',
      'browser/extensions/api/automation/automation_apitest.cc',
      'browser/extensions/api/autotest_private/autotest_private_apitest.cc',
      'browser/extensions/api/bluetooth_low_energy/bluetooth_low_energy_apitest.cc',
      'browser/extensions/api/bluetooth_low_energy/bluetooth_low_energy_apitest_chromeos.cc',
      'browser/extensions/api/bookmark_manager_private/bookmark_manager_private_apitest.cc',
      'browser/extensions/api/bookmarks/bookmark_apitest.cc',
      'browser/extensions/api/braille_display_private/braille_display_private_apitest.cc',
      'browser/extensions/api/braille_display_private/mock_braille_controller.cc',
      'browser/extensions/api/braille_display_private/mock_braille_controller.h',
      'browser/extensions/api/browser/browser_apitest.cc',
      'browser/extensions/api/browsing_data/browsing_data_test.cc',
      'browser/extensions/api/cast_streaming/cast_streaming_apitest.cc',
      'browser/extensions/api/cloud_print_private/cloud_print_private_apitest.cc',
      'browser/extensions/api/command_line_private/command_line_private_apitest.cc',
      'browser/extensions/api/commands/command_service_browsertest.cc',
      'browser/extensions/api/content_settings/content_settings_apitest.cc',
      'browser/extensions/api/context_menus/context_menu_apitest.cc',
      'browser/extensions/api/cookies/cookies_apitest.cc',
      'browser/extensions/api/debugger/debugger_apitest.cc',
      'browser/extensions/api/debugger/debugger_extension_apitest.cc',
      'browser/extensions/api/declarative/declarative_apitest.cc',
      'browser/extensions/api/declarative_content/declarative_content_apitest.cc',
      'browser/extensions/api/declarative_content/request_content_script_apitest.cc',
      'browser/extensions/api/declarative_content/set_icon_apitest.cc',
      'browser/extensions/api/desktop_capture/desktop_capture_apitest.cc',
      'browser/extensions/api/developer_private/developer_private_apitest.cc',
      'browser/extensions/api/dial/dial_apitest.cc',
      'browser/extensions/api/downloads/downloads_api_browsertest.cc',
      'browser/extensions/api/enterprise_device_attributes/enterprise_device_attributes_apitest.cc',
      'browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_apitest_nss.cc',
      'browser/extensions/api/extension_action/browser_action_apitest.cc',
      'browser/extensions/api/extension_action/browser_action_browsertest.cc',
      'browser/extensions/api/extension_action/page_action_apitest.cc',
      'browser/extensions/api/feedback_private/feedback_browsertest.cc',
      'browser/extensions/api/feedback_private/feedback_private_apitest.cc',
      'browser/extensions/api/file_system/file_system_apitest.cc',
      'browser/extensions/api/file_system/file_system_apitest_chromeos.cc',
      'browser/extensions/api/font_settings/font_settings_apitest.cc',
      'browser/extensions/api/gcm/gcm_apitest.cc',
      'browser/extensions/api/history/history_apitest.cc',
      'browser/extensions/api/hotword_private/hotword_private_apitest.cc',
      'browser/extensions/api/i18n/i18n_apitest.cc',
      'browser/extensions/api/identity/identity_apitest.cc',
      'browser/extensions/api/idltest/idltest_apitest.cc',
      'browser/extensions/api/image_writer_private/image_writer_private_apitest.cc',
      'browser/extensions/api/image_writer_private/test_utils.cc',
      'browser/extensions/api/inline_install_private/inline_install_private_apitest.cc',
      'browser/extensions/api/input_ime/input_ime_apitest_chromeos.cc',
      'browser/extensions/api/instance_id/instance_id_apitest.cc',
      'browser/extensions/api/log_private/log_private_apitest_chromeos.cc',
      'browser/extensions/api/management/management_api_browsertest.cc',
      'browser/extensions/api/management/management_apitest.cc',
      'browser/extensions/api/management/management_browsertest.cc',
      'browser/extensions/api/media_galleries/media_galleries_apitest.cc',
      'browser/extensions/api/media_galleries/media_galleries_watch_apitest.cc',
      'browser/extensions/api/messaging/native_messaging_apitest.cc',
      'browser/extensions/api/metrics_private/metrics_apitest.cc',
      'browser/extensions/api/module/module_apitest.cc',
      'browser/extensions/api/music_manager_private/music_manager_private_browsertest.cc',
      'browser/extensions/api/networking_config_chromeos_apitest_chromeos.cc',
      'browser/extensions/api/notification_provider/notification_provider_apitest.cc',
      'browser/extensions/api/omnibox/omnibox_api_browsertest.cc',
      'browser/extensions/api/page_capture/page_capture_apitest.cc',
      'browser/extensions/api/passwords_private/passwords_private_apitest.cc',
      'browser/extensions/api/permissions/permissions_apitest.cc',
      'browser/extensions/api/platform_keys/platform_keys_apitest_nss.cc',
      'browser/extensions/api/preference/preference_apitest.cc',
      'browser/extensions/api/processes/processes_apitest.cc',
      'browser/extensions/api/proxy/proxy_apitest.cc',
      'browser/extensions/api/resources_private/resources_private_apitest.cc',
      'browser/extensions/api/screenlock_private/screenlock_private_apitest.cc',
      'browser/extensions/api/sessions/sessions_apitest.cc',
      'browser/extensions/api/settings_overrides/settings_overrides_browsertest.cc',
      'browser/extensions/api/settings_private/settings_private_apitest.cc',
      'browser/extensions/api/socket/socket_apitest.cc',
      'browser/extensions/api/storage/settings_apitest.cc',
      'browser/extensions/api/streams_private/streams_private_apitest.cc',
      'browser/extensions/api/sync_file_system/sync_file_system_apitest.cc',
      'browser/extensions/api/sync_file_system/sync_file_system_browsertest.cc',
      'browser/extensions/api/system_indicator/system_indicator_apitest.cc',
      'browser/extensions/api/system_private/system_private_apitest.cc',
      'browser/extensions/api/tab_capture/tab_capture_apitest.cc',
      'browser/extensions/api/tabs/tabs_test.cc',
      'browser/extensions/api/terminal/terminal_private_apitest.cc',
      'browser/extensions/api/test/apitest_apitest.cc',
      'browser/extensions/api/top_sites/top_sites_apitest.cc',
      'browser/extensions/api/web_navigation/web_navigation_apitest.cc',
      'browser/extensions/api/web_request/web_request_apitest.cc',
      'browser/extensions/api/webrtc_audio_private/webrtc_audio_private_browsertest.cc',
      'browser/extensions/api/webrtc_from_web_accessible_resource_browsertest.cc',
      'browser/extensions/api/webrtc_logging_private/webrtc_event_log_apitest.cc',
      'browser/extensions/api/webrtc_logging_private/webrtc_logging_private_apitest.cc',
      'browser/extensions/api/webstore_private/webstore_private_apitest.cc',
      'browser/extensions/app_background_page_apitest.cc',
      'browser/extensions/app_process_apitest.cc',
      'browser/extensions/app_window_overrides_browsertest.cc',
      'browser/extensions/background_app_browsertest.cc',
      'browser/extensions/background_page_apitest.cc',
      'browser/extensions/background_scripts_apitest.cc',
      'browser/extensions/background_xhr_browsertest.cc',
      'browser/extensions/browsertest_util_browsertest.cc',
      'browser/extensions/chrome_app_api_browsertest.cc',
      'browser/extensions/chrome_ui_overrides_browsertest.cc',
      'browser/extensions/content_capabilities_browsertest.cc',
      'browser/extensions/content_script_apitest.cc',
      'browser/extensions/content_security_policy_apitest.cc',
      'browser/extensions/content_verifier_browsertest.cc',
      'browser/extensions/crazy_extension_browsertest.cc',
      'browser/extensions/cross_origin_xhr_apitest.cc',
      'browser/extensions/crx_installer_browsertest.cc',
      'browser/extensions/docs/examples/apps/calculator_browsertest.cc',
      'browser/extensions/error_console/error_console_browsertest.cc',
      'browser/extensions/events_apitest.cc',
      'browser/extensions/execute_script_apitest.cc',
      'browser/extensions/extension_action_runner_browsertest.cc',
      'browser/extensions/extension_bindings_apitest.cc',
      'browser/extensions/extension_context_menu_browsertest.cc',
      'browser/extensions/extension_disabled_ui_browsertest.cc',
      'browser/extensions/extension_dom_clipboard_apitest.cc',
      'browser/extensions/extension_fileapi_apitest.cc',
      'browser/extensions/extension_functional_browsertest.cc',
      'browser/extensions/extension_geolocation_apitest.cc',
      'browser/extensions/extension_get_views_apitest.cc',
      'browser/extensions/extension_icon_source_apitest.cc',
      'browser/extensions/extension_incognito_apitest.cc',
      'browser/extensions/extension_install_prompt_browsertest.cc',
      'browser/extensions/extension_install_prompt_test_helper.cc',
      'browser/extensions/extension_install_prompt_test_helper.h',
      'browser/extensions/extension_install_ui_browsertest.cc',
      'browser/extensions/extension_javascript_url_apitest.cc',
      'browser/extensions/extension_loading_browsertest.cc',
      'browser/extensions/extension_management_test_util.cc',
      'browser/extensions/extension_management_test_util.h',
      'browser/extensions/extension_messages_apitest.cc',
      'browser/extensions/extension_override_apitest.cc',
      'browser/extensions/extension_request_limiting_throttle_browsertest.cc',
      'browser/extensions/extension_resource_request_policy_apitest.cc',
      'browser/extensions/extension_startup_browsertest.cc',
      'browser/extensions/extension_storage_apitest.cc',
      'browser/extensions/extension_storage_monitor_browsertest.cc',
      'browser/extensions/extension_tab_util_browsertest.cc',
      'browser/extensions/extension_tabs_apitest.cc',
      'browser/extensions/extension_url_rewrite_browsertest.cc',
      'browser/extensions/extension_view_host_factory_browsertest.cc',
      'browser/extensions/extension_websocket_apitest.cc',
      'browser/extensions/extension_webui_apitest.cc',
      'browser/extensions/fetch_apitest.cc',
      'browser/extensions/gpu_browsertest.cc',
      'browser/extensions/hotword_browsertest.cc',
      'browser/extensions/isolated_app_browsertest.cc',
      'browser/extensions/lazy_background_page_apitest.cc',
      'browser/extensions/lazy_background_page_test_util.h',
      'browser/extensions/mutation_observers_apitest.cc',
      'browser/extensions/options_page_apitest.cc',
      'browser/extensions/page_action_browsertest.cc',
      'browser/extensions/process_management_browsertest.cc',
      'browser/extensions/process_manager_browsertest.cc',
      'browser/extensions/requirements_checker_browsertest.cc',
      'browser/extensions/renderer_initialization_browsertest.cc',
      'browser/extensions/sandboxed_pages_apitest.cc',
      'browser/extensions/service_worker_apitest.cc',
      'browser/extensions/shared_module_apitest.cc',
      'browser/extensions/signin/gaia_auth_extension_loader_browsertest.cc',
      'browser/extensions/startup_helper_browsertest.cc',
      'browser/extensions/stubs_apitest.cc',
      'browser/extensions/subscribe_page_action_browsertest.cc',
      'browser/extensions/wake_event_page_apitest.cc',
      'browser/extensions/web_contents_browsertest.cc',
      'browser/extensions/webstore_inline_installer_browsertest.cc',
      'browser/extensions/webstore_installer_browsertest.cc',
      'browser/extensions/webstore_installer_test.cc',
      'browser/extensions/webstore_installer_test.h',
      'browser/extensions/webstore_reinstaller_browsertest.cc',
      'browser/extensions/webstore_startup_installer_browsertest.cc',
      'browser/extensions/window_open_apitest.cc',
      'browser/external_extension_browsertest.cc',
      'browser/fast_shutdown_browsertest.cc',
      'browser/favicon/content_favicon_driver_browsertest.cc',
      'browser/first_run/first_run_browsertest.cc',
      'browser/first_run/try_chrome_dialog_view_browsertest.cc',
      'browser/geolocation/access_token_store_browsertest.cc',
      'browser/geolocation/geolocation_browsertest.cc',
      'browser/global_keyboard_shortcuts_mac_browsertest.mm',
      'browser/history/history_browsertest.cc',
      'browser/history/redirect_browsertest.cc',
      'browser/iframe_browsertest.cc',
      'browser/image_decoder_browsertest.cc',
      'browser/importer/edge_importer_browsertest_win.cc',
      'browser/importer/firefox_importer_browsertest.cc',
      'browser/importer/ie_importer_browsertest_win.cc',
      'browser/importer/importer_unittest_utils.cc',
      'browser/importer/importer_unittest_utils.h',
      'browser/infobars/infobars_browsertest.cc',
      'browser/interstitials/security_interstitial_page_test_utils.cc',
      'browser/interstitials/security_interstitial_page_test_utils.h',
      'browser/invalidation/profile_invalidation_provider_factory_browsertest.cc',
      'browser/lifetime/browser_close_manager_browsertest.cc',
      'browser/loadtimes_extension_bindings_browsertest.cc',
      'browser/locale_tests_browsertest.cc',
      'browser/media/defer_background_media_browsertest.cc',
      'browser/media/encrypted_media_browsertest.cc',
      'browser/media/encrypted_media_supported_types_browsertest.cc',
      'browser/media/media_browsertest.cc',
      'browser/media/media_browsertest.h',
      'browser/media/media_stream_devices_controller_browsertest.cc',
      'browser/media/media_stream_infobar_browsertest.cc',
      'browser/media/test_license_server.cc',
      'browser/media/test_license_server.h',
      'browser/media/test_license_server_config.h',
      'browser/media/webrtc_apprtc_browsertest.cc',
      'browser/media/webrtc_audio_quality_browsertest.cc',
      'browser/media/webrtc_browsertest.cc',
      'browser/media/webrtc_browsertest_audio.cc',
      'browser/media/webrtc_browsertest_audio.h',
      'browser/media/webrtc_browsertest_base.cc',
      'browser/media/webrtc_browsertest_base.h',
      'browser/media/webrtc_browsertest_common.cc',
      'browser/media/webrtc_browsertest_common.h',
      'browser/media/webrtc_browsertest_perf.cc',
      'browser/media/webrtc_browsertest_perf.h',
      'browser/media/webrtc_disable_encryption_flag_browsertest.cc',
      'browser/media/webrtc_getmediadevices_browsertest.cc',
      'browser/media/webrtc_perf_browsertest.cc',
      'browser/media/webrtc_simulcast_browsertest.cc',
      'browser/media/webrtc_video_quality_browsertest.cc',
      'browser/media/webrtc_webcam_browsertest.cc',
      'browser/media/wv_test_license_server_config.cc',
      'browser/media/wv_test_license_server_config.h',
      'browser/media_galleries/fileapi/iapps_finder_impl_win_browsertest.cc',
      'browser/media_galleries/fileapi/media_file_validator_browsertest.cc',
      'browser/media_galleries/media_galleries_dialog_controller_mock.cc',
      'browser/media_galleries/media_galleries_dialog_controller_mock.h',
      'browser/memory/tab_manager_browsertest.cc',
      'browser/metrics/metrics_memory_details_browsertest.cc',
      'browser/metrics/metrics_service_browsertest.cc',
      'browser/net/cookie_policy_browsertest.cc',
      'browser/net/dns_probe_browsertest.cc',
      'browser/net/ftp_browsertest.cc',
      'browser/net/load_timing_browsertest.cc',
      'browser/net/nss_context_chromeos_browsertest.cc',
      "browser/net/spdyproxy/chrome_data_use_group_browsertest.cc",
      'browser/net/predictor_browsertest.cc',
      'browser/net/proxy_browsertest.cc',
      'browser/net/sdch_browsertest.cc',
      'browser/net/websocket_browsertest.cc',
      'browser/page_load_metrics/page_load_metrics_browsertest.cc',
      'browser/page_load_metrics/observers/https_engagement_page_load_metrics_observer_browsertest.cc',
      'browser/password_manager/credential_manager_browsertest.cc',
      'browser/password_manager/password_manager_browsertest.cc',
      'browser/pdf/pdf_extension_test.cc',
      'browser/pdf/pdf_extension_test_util.cc',
      'browser/pdf/pdf_extension_test_util.h',
      'browser/plugins/plugin_power_saver_browsertest.cc',
      'browser/policy/cloud/cloud_policy_browsertest.cc',
      'browser/policy/cloud/cloud_policy_manager_browsertest.cc',
      'browser/policy/cloud/component_cloud_policy_browsertest.cc',
      'browser/policy/cloud/device_management_service_browsertest.cc',
      'browser/policy/cloud/test_request_interceptor.cc',
      'browser/policy/cloud/test_request_interceptor.h',
      'browser/policy/policy_browsertest.cc',
      'browser/policy/policy_network_browsertest.cc',
      'browser/policy/policy_prefs_browsertest.cc',
      'browser/policy/policy_startup_browsertest.cc',
      'browser/prefetch/prefetch_browsertest.cc',
      'browser/prefs/pref_functional_browsertest.cc',
      'browser/prefs/pref_service_browsertest.cc',
      'browser/prefs/synced_pref_change_registrar_browsertest.cc',
      'browser/prefs/tracked/pref_hash_browsertest.cc',
      'browser/prerender/prerender_browsertest.cc',
      'browser/printing/cloud_print/test/cloud_print_policy_browsertest.cc',
      'browser/printing/cloud_print/test/cloud_print_proxy_process_browsertest.cc',
      'browser/printing/print_preview_dialog_controller_browsertest.cc',
      'browser/printing/print_preview_pdf_generated_browsertest.cc',
      'browser/process_singleton_browsertest.cc',
      'browser/profile_resetter/profile_resetter_browsertest.cc',
      'browser/profiles/host_zoom_map_browsertest.cc',
      'browser/profiles/profile_browsertest.cc',
      'browser/profiles/profile_list_desktop_browsertest.cc',
      'browser/profiles/profile_manager_browsertest.cc',
      'browser/profiles/profile_statistics_browsertest.cc',
      'browser/profiles/profile_window_browsertest.cc',
      'browser/push_messaging/push_messaging_browsertest.cc',
      'browser/referrer_policy_browsertest.cc',
      'browser/renderer_context_menu/mock_render_view_context_menu.cc',
      'browser/renderer_context_menu/mock_render_view_context_menu.h',
      'browser/renderer_context_menu/render_view_context_menu_browsertest.cc',
      'browser/renderer_context_menu/render_view_context_menu_browsertest_util.cc',
      'browser/renderer_context_menu/render_view_context_menu_browsertest_util.h',
      'browser/renderer_context_menu/render_view_context_menu_test_util.cc',
      'browser/renderer_context_menu/render_view_context_menu_test_util.h',
      'browser/renderer_context_menu/spelling_menu_observer_browsertest.cc',
      'browser/renderer_host/chrome_resource_dispatcher_host_delegate_browsertest.cc',
      'browser/renderer_host/render_process_host_chrome_browsertest.cc',
      'browser/repost_form_warning_browsertest.cc',
      'browser/safe_browsing/srt_fetcher_browsertest_win.cc',
      'browser/safe_json_parser_browsertest.cc',
      'browser/search/hotword_installer_browsertest.cc',
      'browser/search/suggestions/image_fetcher_impl_browsertest.cc',
      'browser/search_engines/template_url_scraper_browsertest.cc',
      'browser/service_process/service_process_control_browsertest.cc',
      'browser/services/gcm/fake_gcm_profile_service.cc',
      'browser/services/gcm/fake_gcm_profile_service.h',
      'browser/sessions/better_session_restore_browsertest.cc',
      'browser/sessions/persistent_tab_restore_service_browsertest.cc',
      'browser/sessions/session_restore_browsertest.cc',
      'browser/sessions/session_restore_browsertest_chromeos.cc',
      'browser/sessions/tab_restore_browsertest.cc',
      'browser/site_details_browsertest.cc',
      'browser/spellchecker/spellcheck_service_browsertest.cc',
      'browser/ssl/captive_portal_blocking_page_browsertest.cc',
      'browser/ssl/cert_verifier_browser_test.cc',
      'browser/ssl/cert_verifier_browser_test.h',
      'browser/ssl/certificate_reporting_test_utils.cc',
      'browser/ssl/certificate_reporting_test_utils.h',
      'browser/ssl/chrome_security_state_model_client_browser_tests.cc',
      'browser/ssl/chrome_ssl_host_state_delegate_test.cc',
      'browser/ssl/ssl_browser_tests.cc',
      'browser/ssl/ssl_client_certificate_selector_test.cc',
      'browser/ssl/ssl_client_certificate_selector_test.h',
      'browser/storage/durable_storage_browsertest.cc',
      'browser/subresource_filter/subresource_filter_browsertest.cc',
      'browser/sync_file_system/mock_local_change_processor.cc',
      'browser/sync_file_system/mock_local_change_processor.h',
      'browser/sync_file_system/mock_remote_file_sync_service.cc',
      'browser/sync_file_system/mock_remote_file_sync_service.h',
      'browser/tab_contents/view_source_browsertest.cc',
      'browser/task_management/mock_web_contents_task_manager.cc',
      'browser/task_management/mock_web_contents_task_manager.h',
      'browser/task_management/providers/web_contents/background_contents_tag_browsertest.cc',
      'browser/task_management/providers/web_contents/devtools_tag_browsertest.cc',
      'browser/task_management/providers/web_contents/extension_tag_browsertest.cc',
      'browser/task_management/providers/web_contents/subframe_task_browsertest.cc',
      'browser/task_management/providers/web_contents/tab_contents_tag_browsertest.cc',
      'browser/task_management/task_manager_browsertest.cc',
      'browser/task_management/task_manager_browsertest_util.cc',
      'browser/task_management/task_manager_browsertest_util.h',
      'browser/task_management/task_manager_tester.cc',
      'browser/task_management/task_manager_tester.h',
      'browser/themes/theme_service_browsertest.cc',
      'browser/tracing/chrome_tracing_delegate_browsertest.cc',
      'browser/translate/translate_browsertest.cc',
      'browser/translate/translate_manager_browsertest.cc',
      'browser/ui/autofill/card_unmask_prompt_view_browsertest.cc',
      'browser/ui/autofill/card_unmask_prompt_view_tester.h',
      'browser/ui/autofill/password_generation_popup_view_browsertest.cc',
      'browser/ui/autofill/password_generation_popup_view_tester.h',
      'browser/ui/blocked_content/popup_blocker_browsertest.cc',
      'browser/ui/bookmarks/bookmark_browsertest.cc',
      'browser/ui/browser_browsertest.cc',
      'browser/ui/browser_command_controller_browsertest.cc',
      'browser/ui/browser_navigator_browsertest.cc',
      'browser/ui/browser_navigator_browsertest.h',
      'browser/ui/browser_navigator_browsertest_chromeos.cc',
      'browser/ui/browser_tabrestore_browsertest.cc',
      'browser/ui/content_settings/content_setting_bubble_model_browsertest.cc',
      'browser/ui/content_settings/content_setting_image_model_browsertest.cc',
      'browser/ui/exclusive_access/fullscreen_controller_browsertest.cc',
      'browser/ui/extensions/extension_installed_bubble_browsertest.cc',
      'browser/ui/extensions/extension_message_bubble_browsertest.cc',
      'browser/ui/extensions/extension_message_bubble_browsertest.h',
      'browser/ui/extensions/hosted_app_browsertest.cc',
      'browser/ui/find_bar/find_bar_host_browsertest.cc',
      'browser/ui/location_bar/location_bar_browsertest.cc',
      'browser/ui/login/login_handler_browsertest.cc',
      'browser/ui/native_window_tracker_browsertest.cc',
      'browser/ui/panels/panel_extension_browsertest.cc',
      'browser/ui/passwords/manage_passwords_test.cc',
      'browser/ui/prefs/prefs_tab_helper_browsertest.cc',
      'browser/ui/profile_error_browsertest.cc',
      'browser/ui/search/new_tab_page_interceptor_browsertest.cc',
      'browser/ui/search_engines/search_engine_tab_helper_browsertest.cc',
      'browser/ui/settings_window_manager_browsertest.cc',
      'browser/ui/startup/startup_browser_creator_browsertest.cc',
      'browser/ui/startup/startup_browser_creator_triggered_reset_browsertest_win.cc',
      'browser/ui/sync/one_click_signin_links_delegate_impl_browsertest.cc',
      'browser/ui/sync/profile_signin_confirmation_helper_browsertest.cc',
      'browser/ui/tab_modal_confirm_dialog_browsertest.cc',
      'browser/ui/tab_modal_confirm_dialog_browsertest.h',
      'browser/ui/toolbar/browser_actions_bar_browsertest.cc',
      'browser/ui/toolbar/browser_actions_bar_browsertest.h',
      'browser/ui/toolbar/component_toolbar_actions_browsertest.cc',
      'browser/ui/toolbar/mock_component_toolbar_actions_factory.cc',
      'browser/ui/toolbar/mock_component_toolbar_actions_factory.h',
      'browser/ui/website_settings/mock_permission_bubble_factory.cc',
      'browser/ui/website_settings/mock_permission_bubble_factory.h',
      'browser/ui/website_settings/mock_permission_bubble_view.cc',
      'browser/ui/website_settings/mock_permission_bubble_view.h',
      'browser/ui/website_settings/permission_bubble_browser_test_util.cc',
      'browser/ui/website_settings/permission_bubble_browser_test_util.h',
      'browser/ui/website_settings/permission_bubble_manager_browsertest.cc',
      'browser/ui/webui/bidi_checker_web_ui_test.cc',
      'browser/ui/webui/bidi_checker_web_ui_test.h',
      'browser/ui/webui/bookmarks_ui_browsertest.cc',
      'browser/ui/webui/chrome_url_data_manager_browsertest.cc',
      'browser/ui/webui/chromeos/bluetooth_pairing_ui_browsertest-inl.h',
      'browser/ui/webui/constrained_web_dialog_ui_browsertest.cc',
      'browser/ui/webui/extensions/extension_settings_browsertest.cc',
      'browser/ui/webui/extensions/extension_settings_browsertest.h',
      'browser/ui/webui/identity_internals_ui_browsertest.cc',
      'browser/ui/webui/identity_internals_ui_browsertest.h',
      'browser/ui/webui/inspect_ui_browsertest.cc',
      'browser/ui/webui/interstitials/interstitial_ui_browsertest.cc',
      'browser/ui/webui/log_web_ui_url_browsertest.cc',
      'browser/ui/webui/net_internals/net_internals_ui_browsertest.cc',
      'browser/ui/webui/net_internals/net_internals_ui_browsertest.h',
      'browser/ui/webui/ntp/new_tab_ui_browsertest.cc',
      'browser/ui/webui/options/certificate_manager_browsertest.cc',
      'browser/ui/webui/options/clear_browser_data_browsertest.cc',
      'browser/ui/webui/options/content_settings_exception_area_browsertest.cc',
      'browser/ui/webui/options/multilanguage_options_browsertest.cc',
      'browser/ui/webui/options/multilanguage_options_browsertest.h',
      'browser/ui/webui/options/options_browsertest.cc',
      'browser/ui/webui/options/options_ui_browsertest.cc',
      'browser/ui/webui/options/options_ui_browsertest.h',
      'browser/ui/webui/options/preferences_browsertest.cc',
      'browser/ui/webui/options/preferences_browsertest.h',
      'browser/ui/webui/password_manager_internals/password_manager_internals_ui_browsertest.cc',
      'browser/ui/webui/policy_ui_browsertest.cc',
      'browser/ui/webui/print_preview/print_preview_ui_browsertest.cc',
      'browser/ui/webui/set_as_default_browser_ui_browsertest_win.cc',
      'browser/ui/webui/settings/md_settings_ui_browsertest.cc',
      'browser/ui/webui/signin/inline_login_ui_browsertest.cc',
      'browser/ui/webui/signin/user_manager_ui_browsertest.cc',
      'browser/ui/webui/uber/uber_ui_browsertest.cc',
      'browser/ui/webui/web_ui_test_handler.cc',
      'browser/ui/webui/web_ui_test_handler.h',
      'browser/ui/webui/webui_browsertest.cc',
      'browser/ui/webui/webui_webview_browsertest.cc',
      'browser/ui/zoom/zoom_controller_browsertest.cc',
      'browser/unload_browsertest.cc',
      'browser/web_bluetooth_browsertest.cc',
      'common/mac/app_mode_chrome_locator_browsertest.mm',
      'common/mac/mock_launchd.cc',
      'common/mac/mock_launchd.h',
      'common/time_format_browsertest.cc',
      'renderer/autofill/autofill_renderer_browsertest.cc',
      'renderer/autofill/form_autocomplete_browsertest.cc',
      'renderer/autofill/form_autofill_browsertest.cc',
      'renderer/autofill/form_classifier_browsertest.cc',
      'renderer/autofill/page_click_tracker_browsertest.cc',
      'renderer/autofill/password_autofill_agent_browsertest.cc',
      'renderer/autofill/password_generation_agent_browsertest.cc',
      'renderer/autofill/password_generation_test_utils.cc',
      'renderer/autofill/password_generation_test_utils.h',
      'renderer/chrome_content_renderer_client_browsertest.cc',
      'renderer/chrome_render_frame_observer_browsertest.cc',
      'renderer/content_settings_observer_browsertest.cc',
      'renderer/media/cast_session_browsertest.cc',
      'renderer/translate/translate_helper_browsertest.cc',
      'renderer/translate/translate_script_browsertest.cc',
      'test/base/chrome_render_view_test.cc',
      'test/base/chrome_render_view_test.h',
      'test/base/in_process_browser_test_browsertest.cc',
      'test/base/javascript_browser_test.cc',
      'test/base/javascript_browser_test.h',
      'test/base/test_chrome_web_ui_controller_factory.cc',
      'test/base/test_chrome_web_ui_controller_factory.h',
      'test/base/test_chrome_web_ui_controller_factory_browsertest.cc',
      'test/base/tracing_browsertest.cc',
      'test/base/web_ui_browser_test.cc',
      'test/base/web_ui_browser_test.h',
      'test/base/web_ui_browser_test_browsertest.cc',
      'test/data/webui/async_gen.cc',
      'test/data/webui/async_gen.h',
      'test/data/webui/certificate_viewer_ui_test-inl.h',
      'test/data/webui/chrome_send_browsertest.cc',
      'test/data/webui/chrome_send_browsertest.h',
      'test/data/webui/history_ui_browsertest.cc',
      'test/data/webui/history_ui_browsertest.h',
      'test/data/webui/ntp4_browsertest.cc',
      'test/data/webui/ntp4_browsertest.h',
      'test/data/webui/print_preview.cc',
      'test/data/webui/print_preview.h',
      'test/data/webui/webui_resource_browsertest.cc',
      'test/gpu/webgl_infobar_browsertest.cc',
      'test/ppapi/ppapi_browsertest.cc',
      'test/ppapi/ppapi_filechooser_browsertest.cc',
    ],
    # Tests for Mac only (Cocoa and mac_views_browser=1).
    'chrome_browser_tests_mac_sources': [
      'browser/renderer_host/chrome_render_widget_host_view_mac_history_swiper_browsertest.mm',
      'browser/spellchecker/spellcheck_message_filter_platform_mac_browsertest.cc',
      'browser/ui/cocoa/certificate_viewer_mac_browsertest.mm',
    ],
    # Tests corresponding to the files in chrome_browser_ui_cocoa_sources.
    # Built on Mac, except when mac_views_browser==1.
    'chrome_browser_tests_cocoa_sources': [
      'browser/task_manager/legacy_task_manager_tester.cc',
      'browser/ui/cocoa/accelerators_cocoa_browsertest.mm',
      'browser/ui/cocoa/applescript/browsercrapplication+applescript_test.mm',
      'browser/ui/cocoa/applescript/window_applescript_test.mm',
      'browser/ui/cocoa/apps/app_shim_menu_controller_mac_browsertest.mm',
      'browser/ui/cocoa/apps/native_app_window_cocoa_browsertest.mm',
      'browser/ui/cocoa/browser_window_controller_browsertest.mm',
      'browser/ui/cocoa/constrained_window/constrained_window_mac_browsertest.mm',
      'browser/ui/cocoa/content_settings/collected_cookies_mac_browsertest.mm',
      'browser/ui/cocoa/content_settings/content_setting_bubble_cocoa_browsertest.mm',
      'browser/ui/cocoa/dev_tools_controller_browsertest.mm',
      'browser/ui/cocoa/extensions/extension_install_dialog_controller_browsertest.mm',
      'browser/ui/cocoa/extensions/extension_install_prompt_test_utils.h',
      'browser/ui/cocoa/extensions/extension_install_prompt_test_utils.mm',
      'browser/ui/cocoa/extensions/extension_message_bubble_browsertest_mac.mm',
      'browser/ui/cocoa/extensions/media_galleries_dialog_cocoa_browsertest.mm',
      'browser/ui/cocoa/extensions/windowed_install_dialog_controller_browsertest.mm',
      'browser/ui/cocoa/find_bar/find_bar_browsertest.mm',
      'browser/ui/cocoa/location_bar/zoom_decoration_browsertest.mm',
      'browser/ui/cocoa/omnibox/omnibox_view_mac_browsertest.mm',
      'browser/ui/cocoa/passwords/passwords_bubble_browsertest.mm',
      'browser/ui/cocoa/profiles/profile_signin_confirmation_view_controller_browsertest.mm',
      'browser/ui/cocoa/renderer_context_menu/render_view_context_menu_mac_browsertest.mm',
      'browser/ui/cocoa/ssl_client_certificate_selector_cocoa_browsertest.mm',
      'browser/ui/cocoa/view_id_util_browsertest.mm',
      'browser/ui/cocoa/web_contents_modal_dialog_manager_views_mac_browsertest.mm',
      'browser/ui/cocoa/website_settings/permission_bubble_cocoa_browser_test.mm',
    ],
    'chrome_browser_app_list_ash_test_support_sources': [
      'browser/ui/ash/app_list/test/app_list_service_ash_test_api.cc',
      'browser/ui/ash/app_list/test/app_list_service_ash_test_api.h',
    ],
    'chrome_browser_tests_ash_sources': [
      'browser/ui/ash/accelerator_commands_browsertest.cc',
      'browser/ui/ash/accelerator_controller_browsertest.cc',
      'browser/ui/ash/chrome_new_window_delegate_browsertest.cc',
      'browser/ui/ash/keyboard_controller_browsertest.cc',
      'browser/ui/ash/launcher/chrome_launcher_controller_impl_browsertest.cc',
      'browser/ui/ash/launcher/launcher_favicon_loader_browsertest.cc',
      'browser/ui/ash/multi_user/multi_user_window_manager_test.cc',
      'browser/ui/ash/multi_user/multi_user_window_manager_test.h',
      'browser/ui/ash/shelf_browsertest.cc',
      'browser/ui/ash/system_tray_delegate_chromeos_browsertest_chromeos.cc',
      'browser/ui/ash/system_tray_tray_cast_browsertest_chromeos.cc',
      'browser/ui/ash/system_tray_tray_cast_browsertest_media_router_chromeos.cc',
      'browser/ui/ash/volume_controller_browsertest_chromeos.cc',
      'browser/ui/views/frame/browser_non_client_frame_view_ash_browsertest.cc',
    ],
    # Cross-platform views browser tests ready for toolkit-views on Mac.
    'chrome_browser_tests_views_sources': [
      'browser/ui/global_error/global_error_service_browsertest.cc',
      'browser/ui/views/extensions/extension_uninstall_dialog_view_browsertest.cc',
      'browser/ui/views/frame/browser_non_client_frame_view_browsertest_win.cc',
      'browser/ui/views/frame/browser_window_property_manager_browsertest_win.cc',
      'browser/ui/views/select_file_dialog_extension_browsertest.cc',
    ],
    # Cross-platform (except Mac/Cocoa) views browser tests. Excluded on Mac
    # unless building with mac_views_browser=1.
    'chrome_browser_tests_views_non_mac_sources': [
      # TODO(tapted): Move these to chrome_browser_tests_views_sources when the
      # the corresponding files are moved in chrome_browser_ui.gypi (i.e. out of
      # chrome_browser_ui_views_non_mac_sources). http://crbug.com/404979.
      'browser/task_manager/task_manager_tester_nonmac.cc',
      'browser/ui/views/autofill/autofill_popup_base_view_browsertest.cc',
      'browser/ui/views/autofill/card_unmask_prompt_view_tester_views.cc',
      'browser/ui/views/autofill/card_unmask_prompt_view_tester_views.h',
      'browser/ui/views/autofill/password_generation_popup_view_tester_views.cc',
      'browser/ui/views/autofill/password_generation_popup_view_tester_views.h',
      'browser/ui/views/bookmarks/bookmark_bubble_sign_in_delegate_browsertest.cc',
      'browser/ui/views/collected_cookies_views_browsertest.cc',
      'browser/ui/views/extensions/bookmark_override_browsertest.cc',
      'browser/ui/views/extensions/chooser_dialog_view_browsertest.cc',
      'browser/ui/views/extensions/extension_install_dialog_view_browsertest.cc',
      'browser/ui/views/extensions/extension_message_bubble_view_browsertest.cc',
      'browser/ui/views/frame/browser_non_client_frame_view_browsertest.cc',
      'browser/ui/views/frame/browser_view_browsertest.cc',
      'browser/ui/views/location_bar/zoom_bubble_view_browsertest.cc',
      'browser/ui/views/media_router/media_router_ui_browsertest.cc',
      'browser/ui/views/new_task_manager_view_browsertest.cc',
      'browser/ui/views/passwords/password_dialog_view_browsertest.cc',
      'browser/ui/views/toolbar/browser_actions_container_browsertest.cc',
      'browser/ui/views/toolbar/toolbar_view_browsertest.cc',
      'browser/ui/views/translate/translate_bubble_view_browsertest.cc',
      'browser/ui/views/web_dialog_view_browsertest.cc',
    ],
    'chrome_browser_tests_non_mac_desktop_sources': [
      # This test is for the spelling options submenu that's only for Windows,
      # ChromeOS, and Linux.
      'browser/renderer_context_menu/spelling_options_submenu_observer_browsertest.cc',
    ],
    'chrome_browser_tests_chromeos_sources': [
      '../ui/base/ime/chromeos/input_method_whitelist.cc',
      '../ui/base/ime/chromeos/input_method_whitelist.h',
      'browser/apps/custom_launcher_page_browsertest_views.cc',
      'browser/chromeos/accessibility/accessibility_manager_browsertest.cc',
      'browser/chromeos/accessibility/magnification_manager_browsertest.cc',
      'browser/chromeos/accessibility/speech_monitor.cc',
      'browser/chromeos/accessibility/speech_monitor.h',
      'browser/chromeos/accessibility/touch_exploration_controller_browsertest.cc',
      'browser/chromeos/app_mode/kiosk_app_manager_browsertest.cc',
      'browser/chromeos/app_mode/kiosk_app_update_service_browsertest.cc',
      'browser/chromeos/app_mode/kiosk_crash_restore_browsertest.cc',
      'browser/chromeos/arc/arc_auth_service_browsertest.cc',
      'browser/chromeos/attestation/attestation_policy_browsertest.cc',
      'browser/chromeos/customization/customization_document_browsertest.cc',
      'browser/chromeos/customization/customization_wallpaper_downloader_browsertest.cc',
      'browser/chromeos/device/input_service_proxy_browsertest.cc',
      'browser/chromeos/device/input_service_test_helper.cc',
      'browser/chromeos/device/input_service_test_helper.h',
      'browser/chromeos/display/quirks_browsertest.cc',
      'browser/chromeos/drive/drive_integration_service_browsertest.cc',
      'browser/chromeos/extensions/accessibility_features_apitest.cc',
      'browser/chromeos/extensions/echo_private_apitest.cc',
      'browser/chromeos/extensions/file_manager/file_browser_handler_api_test.cc',
      'browser/chromeos/extensions/file_manager/file_manager_private_apitest.cc',
      'browser/chromeos/extensions/file_system_provider/file_system_provider_apitest.cc',
      'browser/chromeos/extensions/info_private_apitest.cc',
      'browser/chromeos/extensions/input_method_apitest_chromeos.cc',
      'browser/chromeos/extensions/users_private/users_private_apitest.cc',
      'browser/chromeos/extensions/virtual_keyboard_browsertest.cc',
      'browser/chromeos/extensions/virtual_keyboard_browsertest.h',
      'browser/chromeos/extensions/wallpaper_apitest.cc',
      'browser/chromeos/extensions/wallpaper_manager_browsertest.cc',
      'browser/chromeos/extensions/wallpaper_private_apitest.cc',
      'browser/chromeos/file_manager/audio_player_browsertest.cc',
      'browser/chromeos/file_manager/external_filesystem_apitest.cc',
      'browser/chromeos/file_manager/file_manager_browsertest.cc',
      'browser/chromeos/file_manager/file_manager_browsertest_base.cc',
      'browser/chromeos/file_manager/file_manager_browsertest_base.h',
      'browser/chromeos/file_manager/file_manager_jstest.cc',
      'browser/chromeos/file_manager/file_manager_jstest_base.cc',
      'browser/chromeos/file_manager/file_manager_jstest_base.h',
      'browser/chromeos/file_manager/gallery_browsertest.cc',
      'browser/chromeos/file_manager/gallery_jstest.cc',
      'browser/chromeos/file_manager/image_loader_jstest.cc',
      'browser/chromeos/file_manager/mount_test_util.cc',
      'browser/chromeos/file_manager/mount_test_util.h',
      'browser/chromeos/file_manager/video_player_browsertest.cc',
      'browser/chromeos/file_manager/zip_file_creator_browsertest.cc',
      'browser/chromeos/first_run/chromeos_first_run_browsertest.cc',
      'browser/chromeos/first_run/drive_first_run_browsertest.cc',
      'browser/chromeos/first_run/goodies_displayer_browsertest.cc',
      'browser/chromeos/input_method/input_method_engine_browsertests.cc',
      'browser/chromeos/input_method/mode_indicator_browsertest.cc',
      'browser/chromeos/login/bluetooth_host_pairing_browsertest.cc',
      'browser/chromeos/login/crash_restore_browsertest.cc',
      'browser/chromeos/login/demo_mode/demo_app_launcher_browsertest.cc',
      'browser/chromeos/login/easy_unlock/bootstrap_browsertest.cc',
      'browser/chromeos/login/enable_debugging_browsertest.cc',
      'browser/chromeos/login/enrollment/enrollment_screen_browsertest.cc',
      'browser/chromeos/login/enrollment/enterprise_enrollment_helper_mock.cc',
      'browser/chromeos/login/enrollment/enterprise_enrollment_helper_mock.h',
      'browser/chromeos/login/enrollment/mock_auto_enrollment_check_screen.cc',
      'browser/chromeos/login/enrollment/mock_auto_enrollment_check_screen.h',
      'browser/chromeos/login/enrollment/mock_enrollment_screen.cc',
      'browser/chromeos/login/enrollment/mock_enrollment_screen.h',
      'browser/chromeos/login/enterprise_enrollment_browsertest.cc',
      'browser/chromeos/login/existing_user_controller_browsertest.cc',
      'browser/chromeos/login/hid_detection_browsertest.cc',
      'browser/chromeos/login/kiosk_browsertest.cc',
      'browser/chromeos/login/lock/screen_locker_tester.cc',
      'browser/chromeos/login/lock/screen_locker_tester.h',
      'browser/chromeos/login/login_manager_test.cc',
      'browser/chromeos/login/login_manager_test.h',
      'browser/chromeos/login/login_screen_policy_browsertest.cc',
      'browser/chromeos/login/login_ui_keyboard_browsertest.cc',
      'browser/chromeos/login/login_utils_browsertest.cc',
      'browser/chromeos/login/mixin_based_browser_test.cc',
      'browser/chromeos/login/mixin_based_browser_test.h',
      'browser/chromeos/login/oobe_localization_browsertest.cc',
      # TODO(nkostylev) Re-enable ResourceLoaderBrowserTest.
      #'browser/chromeos/login/resource_loader_browsertest.cc',
      'browser/chromeos/login/proxy_auth_dialog_browsertest.cc',
      'browser/chromeos/login/reset_browsertest.cc',
      'browser/chromeos/login/saml/saml_browsertest.cc',
      'browser/chromeos/login/screens/hid_detection_screen_browsertest.cc',
      'browser/chromeos/login/screens/mock_base_screen_delegate.cc',
      'browser/chromeos/login/screens/mock_base_screen_delegate.h',
      'browser/chromeos/login/screens/mock_enable_debugging_screen.cc',
      'browser/chromeos/login/screens/mock_enable_debugging_screen.h',
      'browser/chromeos/login/screens/mock_error_screen.cc',
      'browser/chromeos/login/screens/mock_error_screen.h',
      'browser/chromeos/login/screens/mock_eula_screen.cc',
      'browser/chromeos/login/screens/mock_eula_screen.h',
      'browser/chromeos/login/screens/mock_network_screen.cc',
      'browser/chromeos/login/screens/mock_network_screen.h',
      'browser/chromeos/login/screens/mock_update_screen.cc',
      'browser/chromeos/login/screens/mock_update_screen.h',
      'browser/chromeos/login/screens/mock_wrong_hwid_screen.cc',
      'browser/chromeos/login/screens/mock_wrong_hwid_screen.h',
      'browser/chromeos/login/screens/network_screen_browsertest.cc',
      'browser/chromeos/login/screens/update_screen_browsertest.cc',
      'browser/chromeos/login/session_login_browsertest.cc',
      'browser/chromeos/login/signin/device_id_browsertest.cc',
      'browser/chromeos/login/signin/oauth2_browsertest.cc',
      'browser/chromeos/login/supervised/supervised_user_creation_browsertest.cc',
      'browser/chromeos/login/supervised/supervised_user_password_browsertest.cc',
      'browser/chromeos/login/supervised/supervised_user_test_base.cc',
      'browser/chromeos/login/supervised/supervised_user_test_base.h',
      'browser/chromeos/login/test/app_window_waiter.cc',
      'browser/chromeos/login/test/app_window_waiter.h',
      'browser/chromeos/login/test/https_forwarder.cc',
      'browser/chromeos/login/test/https_forwarder.h',
      'browser/chromeos/login/test/oobe_base_test.cc',
      'browser/chromeos/login/test/oobe_base_test.h',
      'browser/chromeos/login/test/wizard_in_process_browser_test.cc',
      'browser/chromeos/login/test/wizard_in_process_browser_test.h',
      'browser/chromeos/login/ui/captive_portal_window_browsertest.cc',
      'browser/chromeos/login/ui/login_feedback_browsertest.cc',
      'browser/chromeos/login/ui/login_web_dialog_browsertest.cc',
      'browser/chromeos/login/ui/simple_web_view_dialog_browsertest.cc',
      'browser/chromeos/login/ui/user_adding_screen_browsertest.cc',
      'browser/chromeos/login/users/avatar/user_image_manager_browsertest.cc',
      'browser/chromeos/login/users/avatar/user_image_manager_test_util.cc',
      'browser/chromeos/login/users/avatar/user_image_manager_test_util.h',
      'browser/chromeos/login/users/wallpaper/wallpaper_manager_policy_browsertest.cc',
      'browser/chromeos/login/users/wallpaper/wallpaper_manager_test_utils.cc',
      'browser/chromeos/login/users/wallpaper/wallpaper_manager_test_utils.h',
      'browser/chromeos/login/webview_login_browsertest.cc',
      'browser/chromeos/login/wizard_controller_browsertest.cc',
      'browser/chromeos/net/network_portal_detector_impl_browsertest.cc',
      'browser/chromeos/ownership/fake_owner_settings_service.cc',
      'browser/chromeos/ownership/fake_owner_settings_service.h',
      'browser/chromeos/policy/affiliation_test_helper.cc',
      'browser/chromeos/policy/affiliation_test_helper.h',
      'browser/chromeos/policy/blocking_login_browsertest.cc',
      'browser/chromeos/policy/device_cloud_policy_browsertest.cc',
      'browser/chromeos/policy/device_local_account_browsertest.cc',
      'browser/chromeos/policy/device_policy_cros_browser_test.cc',
      'browser/chromeos/policy/device_policy_cros_browser_test.h',
      'browser/chromeos/policy/device_quirks_policy_browsertest.cc',
      'browser/chromeos/policy/device_status_collector_browsertest.cc',
      'browser/chromeos/policy/device_system_use_24hour_clock_browsertest.cc',
      'browser/chromeos/policy/display_rotation_default_handler_browsertest.cc',
      'browser/chromeos/policy/force_maximize_on_first_run_chromeos_browsertest.cc',
      'browser/chromeos/policy/login_policy_test_base.cc',
      'browser/chromeos/policy/login_policy_test_base.h',
      'browser/chromeos/policy/login_screen_default_policy_browsertest.cc',
      'browser/chromeos/policy/policy_cert_verifier_browsertest.cc',
      'browser/chromeos/policy/power_policy_browsertest.cc',
      'browser/chromeos/policy/restore_on_startup_browsertest_chromeos.cc',
      'browser/chromeos/policy/user_affiliation_browsertest.cc',
      'browser/chromeos/policy/user_cloud_external_data_manager_browsertest.cc',
      'browser/chromeos/policy/user_cloud_policy_manager_chromeos_browsertest.cc',
      'browser/chromeos/policy/user_policy_test_helper.cc',
      'browser/chromeos/policy/user_policy_test_helper.h',
      'browser/chromeos/policy/variations_service_policy_browsertest.cc',
      'browser/chromeos/power/peripheral_battery_observer_browsertest.cc',
      'browser/chromeos/preferences_chromeos_browsertest.cc',
      'browser/chromeos/profiles/profile_helper_browsertest.cc',
      'browser/chromeos/settings/scoped_cros_settings_test_helper.cc',
      'browser/chromeos/settings/scoped_cros_settings_test_helper.h',
      'browser/chromeos/shutdown_policy_browsertest.cc',
      'browser/chromeos/system/device_disabling_browsertest.cc',
      'browser/chromeos/system/tray_accessibility_browsertest.cc',
      'browser/download/notification/download_notification_browsertest.cc',
      'browser/drive/drive_notification_manager_factory_browsertest.cc',
      'browser/extensions/api/certificate_provider/certificate_provider_apitest.cc',
      'browser/extensions/api/vpn_provider/vpn_provider_apitest.cc',
      'browser/ui/ash/launcher/arc_app_launcher_browsertest.cc',
      'browser/ui/webui/options/chromeos/accounts_options_browsertest.cc',
      'browser/ui/webui/options/chromeos/guest_mode_options_browsertest.cc',
      'browser/ui/webui/options/chromeos/guest_mode_options_ui_browsertest.cc',
      'browser/ui/webui/options/chromeos/shared_options_browsertest.cc',
    ],
    'chrome_browser_tests_views_non_cros_or_mac_sources': [
      # This should be brought up on OSX Views but not CrOS.
      'browser/ui/views/profiles/profile_chooser_view_browsertest.cc',
    ],
    'chrome_browser_tests_speech_sources': [
      'browser/speech/extension_api/tts_extension_apitest.cc',
      'browser/speech/speech_recognition_browsertest.cc',
    ],
    'chrome_browser_tests_full_safe_browsing_sources': [
      'browser/safe_browsing/safe_browsing_blocking_page_test.cc',
      'browser/safe_browsing/safe_browsing_service_browsertest.cc',
      'renderer/safe_browsing/phishing_classifier_browsertest.cc',
      'renderer/safe_browsing/phishing_classifier_delegate_browsertest.cc',
      'renderer/safe_browsing/phishing_dom_feature_extractor_browsertest.cc',
      'renderer/safe_browsing/threat_dom_details_browsertest.cc',
    ],
    'chrome_browser_tests_remoting_sources': [
      'test/remoting/auth_browsertest.cc',
      'test/remoting/fullscreen_browsertest.cc',
      'test/remoting/it2me_browsertest.cc',
      'test/remoting/key_code_conv.cc',
      'test/remoting/key_code_conv.h',
      'test/remoting/key_code_map.h',
      'test/remoting/key_code_test_map.h',
      'test/remoting/launch_browsertest.cc',
      'test/remoting/me2me_browsertest.cc',
      'test/remoting/page_load_notification_observer.cc',
      'test/remoting/page_load_notification_observer.h',
      'test/remoting/pin_browsertest.cc',
      'test/remoting/qunit_browser_test_runner.cc',
      'test/remoting/remote_desktop_browsertest.cc',
      'test/remoting/remote_desktop_browsertest.h',
      'test/remoting/remote_test_helper.cc',
      'test/remoting/remote_test_helper.h',
      'test/remoting/scrollbar_browsertest.cc',
      'test/remoting/unauthenticated_browsertest.cc',
      'test/remoting/waiter.cc',
      'test/remoting/waiter.h',
      'test/remoting/webapp_javascript_browsertest.cc',
    ],
    'chrome_browser_tests_supervised_user_sources': [
      'browser/supervised_user/supervised_user_browsertest.cc',
      'browser/supervised_user/supervised_user_resource_throttle_browsertest.cc',
      'browser/supervised_user/supervised_user_service_browsertest.cc',
    ],
    'chrome_browser_tests_app_list_sources': [
      'browser/apps/drive/drive_app_converter_browsertest.cc',
      'browser/apps/drive/drive_app_provider_browsertest.cc',
      'browser/ui/app_list/app_list_controller_browsertest.cc',
      'browser/ui/app_list/app_list_service_impl_browsertest.cc',
      'browser/ui/app_list/app_list_service_views_browsertest.cc',
      'browser/ui/app_list/search/webstore/webstore_provider_browsertest.cc',
      'browser/ui/app_list/speech_recognizer_browsertest.cc',
    ],
    'chrome_browser_tests_service_discovery_sources': [
      'browser/extensions/api/mdns/mdns_apitest.cc',
    ],
    'chrome_browser_tests_media_router_sources': [
      'browser/ui/webui/media_router/media_router_dialog_controller_impl_browsertest.cc',
      'test/data/webui/media_router/media_router_elements_browsertest.js',
      'test/media_router/media_router_base_browsertest.cc',
      'test/media_router/media_router_base_browsertest.h',
      'test/media_router/media_router_e2e_browsertest.cc',
      'test/media_router/media_router_e2e_browsertest.h',
      'test/media_router/media_router_e2e_ui_browsertest.cc',
      'test/media_router/media_router_integration_browsertest.cc',
      'test/media_router/media_router_integration_browsertest.h',
      'test/media_router/media_router_integration_ui_browsertest.cc',
      'test/media_router/test_media_sinks_observer.cc',
      'test/media_router/test_media_sinks_observer.h',
    ],
    # Javascript sources. These are combined with the .cc files in the GYP build
    # and are handled by a rule, but in the GN build they're in a separate
    # action so need to be separated out.
    'chrome_browser_tests_webui_js_sources': [
      'browser/ui/webui/app_list/start_page_browsertest.js',
      'browser/ui/webui/chromeos/bluetooth_pairing_ui_browsertest.js',
      'browser/ui/webui/chromeos/certificate_manager_dialog_browsertest.js',
      'browser/ui/webui/chromeos/set_time_ui_browsertest.js',
      'browser/ui/webui/extensions/chromeos/kiosk_apps_browsertest.js',
      'browser/ui/webui/extensions/extension_settings_browsertest.js',
      'browser/ui/webui/help/help_browsertest.js',
      'browser/ui/webui/identity_internals_ui_browsertest.js',
      'browser/ui/webui/options/autofill_options_browsertest.js',
      'browser/ui/webui/options/browser_options_browsertest.js',
      'browser/ui/webui/options/certificate_manager_browsertest.js',
      'browser/ui/webui/options/chromeos/accounts_options_browsertest.js',
      'browser/ui/webui/options/chromeos/bluetooth_options_browsertest.js',
      'browser/ui/webui/options/chromeos/date_time_options_browsertest.js',
      'browser/ui/webui/options/chromeos/guest_mode_options_browsertest.js',
      'browser/ui/webui/options/chromeos/power_overlay_browsertest.js',
      'browser/ui/webui/options/content_options_browsertest.js',
      'browser/ui/webui/options/content_settings_exception_area_browsertest.js',
      'browser/ui/webui/options/cookies_view_browsertest.js',
      'browser/ui/webui/options/edit_dictionary_browsertest.js',
      'browser/ui/webui/options/font_settings_browsertest.js',
      'browser/ui/webui/options/language_options_browsertest.js',
      'browser/ui/webui/options/language_options_dictionary_download_browsertest.js',
      'browser/ui/webui/options/manage_profile_browsertest.js',
      'browser/ui/webui/options/multilanguage_options_webui_browsertest.js',
      'browser/ui/webui/options/options_browsertest.js',
      'browser/ui/webui/options/options_browsertest_base.js',
      'browser/ui/webui/options/password_manager_browsertest.js',
      'browser/ui/webui/options/profile_settings_reset_browsertest.js',
      'browser/ui/webui/options/search_engine_manager_browsertest.js',
      'browser/ui/webui/options/settings_app_browsertest.js',
      'browser/ui/webui/options/settings_format_browsertest.js',
      'browser/ui/webui/options/startup_page_list_browsertest.js',
      'browser/ui/webui/sync_internals_browsertest.js',
      'browser/ui/webui/sync_setup_browsertest.js',
      'test/data/chromeos/oobe_webui_browsertest.js',
      'test/data/webui/about_invalidations_browsertest.js',
      'test/data/webui/accessibility_audit_browsertest.js',
      'test/data/webui/assertions.js',
      'test/data/webui/async_gen.js',
      'test/data/webui/certificate_viewer_dialog_test.js',
      'test/data/webui/chrome_send_browsertest.js',
      'test/data/webui/cr_elements/cr_elements_browsertest.js',
      'test/data/webui/extensions/cr_extensions_browsertest.js',
      'test/data/webui/history_browsertest.js',
      'test/data/webui/md_downloads/downloads_browsertest.js',
      'test/data/webui/md_history/md_history_browsertest.js',
      'test/data/webui/md_user_manager/user_manager_browsertest.js',
      'test/data/webui/mock4js_browsertest.js',
      'test/data/webui/net_internals/bandwidth_view.js',
      'test/data/webui/net_internals/chromeos_view.js',
      'test/data/webui/net_internals/dns_view.js',
      'test/data/webui/net_internals/events_view.js',
      'test/data/webui/net_internals/hsts_view.js',
      'test/data/webui/net_internals/log_util.js',
      'test/data/webui/net_internals/log_view_painter.js',
      'test/data/webui/net_internals/main.js',
      'test/data/webui/net_internals/net_internals_test.js',
      'test/data/webui/net_internals/prerender_view.js',
      'test/data/webui/net_internals/sdch_view.js',
      'test/data/webui/net_internals/timeline_view.js',
      'test/data/webui/ntp4.js',
      'test/data/webui/plugins_browsertest.js',
      'test/data/webui/polymer_browser_test_base.js',
      'test/data/webui/print_preview.js',
      'test/data/webui/sandboxstatus_browsertest.js',
      'test/data/webui/settings/advanced_page_browsertest.js',
      'test/data/webui/settings/basic_page_browsertest.js',
      'test/data/webui/settings/bluetooth_page_browsertest_chromeos.js',
      'test/data/webui/settings/cr_settings_browsertest.js',
      'test/data/webui/settings/easy_unlock_browsertest_chromeos.js',
      'test/data/webui/settings/help_page_browsertest.js',
      'test/data/webui/settings/languages_page_browsertest.js',
      'test/data/webui/settings/on_startup_browsertest.js',
      'test/data/webui/settings/passwords_and_autofill_fake_data.js',
      'test/data/webui/settings/passwords_and_forms_browsertest.js',
      'test/data/webui/settings/settings_autofill_section_browsertest.js',
      'test/data/webui/settings/settings_page_browsertest.js',
      'test/data/webui/settings/settings_passwords_section_browsertest.js',
      'test/data/webui/settings/settings_subpage_browsertest.js',
      'test/data/webui/text_defaults_browsertest.js',
      'test/data/webui/webui_resource_async_browsertest.js',
    ],
    # TODO(rockot) bug 505926: These should be moved to a target in
    # //extensions but have old dependencies on chrome files. The chrome
    # dependencies should be removed and these moved to the
    # extensions_browsertests target.
    'chrome_interactive_ui_test_extensions_sources': [
      '../extensions/browser/app_window/app_window_interactive_uitest.cc',
    ],
    'chrome_interactive_ui_test_sources': [
      '../ui/base/clipboard/clipboard_android_unittest.cc',
      '../ui/base/clipboard/clipboard_unittest.cc',
      'browser/apps/app_browsertest_util.cc',
      'browser/apps/app_browsertest_util.h',
      'browser/apps/app_pointer_lock_interactive_uitest.cc',
      'browser/apps/app_shim/app_shim_interactive_uitest_mac.mm',
      'browser/apps/app_shim/app_shim_quit_interactive_uitest_mac.mm',
      'browser/apps/app_window_interactive_uitest.cc',
      'browser/apps/guest_view/web_view_interactive_browsertest.cc',
      'browser/autofill/autofill_interactive_uitest.cc',
      'browser/autofill/autofill_uitest_util.cc',
      'browser/browser_keyevents_browsertest.cc',
      'browser/extensions/api/extension_action/browser_action_interactive_test.cc',
      'browser/extensions/api/omnibox/omnibox_api_interactive_test.cc',
      'browser/extensions/browsertest_util.cc',
      'browser/extensions/extension_apitest.cc',
      'browser/extensions/extension_browsertest.cc',
      'browser/extensions/extension_commands_global_registry_apitest.cc',
      'browser/extensions/extension_crash_recovery_browsertest.cc',
      'browser/extensions/extension_fullscreen_apitest.cc',
      'browser/extensions/extension_function_test_utils.cc',
      'browser/extensions/extension_keybinding_apitest.cc',
      'browser/extensions/extension_test_notification_observer.cc',
      'browser/extensions/updater/extension_cache_fake.cc',
      'browser/extensions/updater/extension_cache_fake.h',
      'browser/extensions/window_open_interactive_apitest.cc',
      'browser/mouseleave_browsertest.cc',
      'browser/password_manager/password_generation_interactive_uitest.cc',
      'browser/password_manager/password_manager_interactive_uitest.cc',
      'browser/renderer_context_menu/render_view_context_menu_browsertest_util.cc',
      'browser/renderer_context_menu/render_view_context_menu_browsertest_util.h',
      'browser/site_per_process_interactive_browsertest.cc',
      'browser/ui/autofill/autofill_popup_controller_interactive_uitest.cc',
      'browser/ui/browser_focus_uitest.cc',
      'browser/ui/cocoa/apps/app_shim_menu_controller_mac_interactive_uitest.mm',
      'browser/ui/cocoa/apps/quit_with_apps_controller_mac_interactive_uitest.mm',
      'browser/ui/cocoa/panels/panel_cocoa_browsertest.mm',
      'browser/ui/exclusive_access/flash_fullscreen_interactive_browsertest.cc',
      'browser/ui/exclusive_access/fullscreen_controller_interactive_browsertest.cc',
      'browser/ui/exclusive_access/fullscreen_controller_state_interactive_browsertest.cc',
      'browser/ui/find_bar/find_bar_host_interactive_uitest.cc',
      'browser/ui/omnibox/omnibox_view_browsertest.cc',
      'browser/ui/panels/base_panel_browser_test.cc',
      'browser/ui/panels/base_panel_browser_test.h',
      'browser/ui/panels/test_panel_active_state_observer.cc',
      'browser/ui/panels/test_panel_active_state_observer.h',
      'browser/ui/panels/test_panel_collection_squeeze_observer.cc',
      'browser/ui/panels/test_panel_collection_squeeze_observer.h',
      'browser/ui/panels/test_panel_mouse_watcher.cc',
      'browser/ui/panels/test_panel_mouse_watcher.h',
      'browser/ui/panels/test_panel_notification_observer.cc',
      'browser/ui/panels/test_panel_notification_observer.h',
      'browser/ui/passwords/manage_passwords_test.cc',
      'browser/ui/passwords/manage_passwords_test.h',
      'browser/ui/search/instant_extended_interactive_uitest.cc',
      'browser/ui/search/instant_extended_manual_interactive_uitest.cc',
      'browser/ui/search/instant_test_utils.cc',
      'browser/ui/search/instant_test_utils.h',
      'browser/ui/search/local_ntp_browsertest.cc',
      'browser/ui/startup/startup_browser_creator_interactive_uitest.cc',
      'browser/ui/translate/translate_bubble_test_utils.h',
      'browser/ui/views/accessibility/navigation_accessibility_uitest_win.cc',
      'browser/ui/webui/options/language_dictionary_interactive_uitest.cc',
      'browser/ui/webui/options/language_options_interactive_uitest.cc',
      'test/base/always_on_top_window_killer_win.cc',
      'test/base/always_on_top_window_killer_win.h',
      'test/base/interactive_test_utils.cc',
      'test/base/interactive_test_utils.h',
      'test/base/interactive_test_utils_aura.cc',
      'test/base/interactive_test_utils_aura.h',
      'test/base/interactive_test_utils_common_views.cc',
      'test/base/interactive_test_utils_mac.mm',
      'test/base/interactive_test_utils_win.cc',
      'test/base/interactive_ui_tests_main.cc',
      'test/base/view_event_test_platform_part.h',
      'test/base/view_event_test_platform_part_chromeos.cc',
      'test/base/view_event_test_platform_part_default.cc',
      'test/ppapi/ppapi_interactive_browsertest.cc',
    ],
    # Panels sources not related to UI toolkit. ChromeOS doesn't use panels.
    'chrome_interactive_ui_test_panels_sources': [
      'browser/ui/panels/detached_panel_browsertest.cc',
      'browser/ui/panels/docked_panel_browsertest.cc',
      'browser/ui/panels/panel_browsertest.cc',
      'browser/ui/panels/panel_resize_browsertest.cc',
    ],
    'chrome_interactive_ui_test_panels_views_sources': [
      'browser/ui/views/panels/panel_view_browsertest.cc',
    ],
     # Cross-platform views interactive tests ready for toolkit-views on Mac.
    'chrome_interactive_ui_test_views_sources': [
      '../ui/views/corewm/desktop_capture_controller_unittest.cc',
      '../ui/views/widget/desktop_aura/desktop_window_tree_host_x11_interactive_uitest.cc',
      '../ui/views/widget/desktop_aura/x11_topmost_window_finder_interactive_uitest.cc',
      '../ui/views/widget/widget_interactive_uitest.cc',
      'browser/ui/views/menu_controller_interactive_uitest.cc',
      'browser/ui/views/menu_item_view_interactive_uitest.cc',
      'browser/ui/views/menu_model_adapter_test.cc',
      'browser/ui/views/menu_test_base.cc',
      'browser/ui/views/menu_test_base.h',
      'browser/ui/views/menu_view_drag_and_drop_test.cc',
      'browser/ui/views/status_icons/status_tray_state_changer_interactive_uitest_win.cc',
      'test/base/view_event_test_base.cc',
      'test/base/view_event_test_base.h',
    ],
    # Mac sources, except when mac_views_browser==1.
    'chrome_interactive_ui_test_cocoa_sources': [
      'browser/ui/cocoa/extensions/browser_action_button_interactive_uitest.mm',
      'browser/ui/cocoa/translate/translate_bubble_test_utils_cocoa.mm',
      'test/base/interactive_test_utils_cocoa.mm',
    ],
    'chrome_interactive_ui_test_app_list_sources': [
      'browser/ui/app_list/app_list_service_interactive_uitest.cc',
      'browser/ui/app_list/app_list_service_mac_interactive_uitest.mm',
    ],
    'chrome_interactive_ui_test_win_sources': [
      '../ui/resources/cursors/aliasb.cur',
      '../ui/resources/cursors/cell.cur',
      '../ui/resources/cursors/col_resize.cur',
      '../ui/resources/cursors/copy.cur',
      '../ui/resources/cursors/none.cur',
      '../ui/resources/cursors/row_resize.cur',
      '../ui/resources/cursors/vertical_text.cur',
      '../ui/resources/cursors/zoom_in.cur',
      '../ui/resources/cursors/zoom_out.cur',
      'app/chrome_dll.rc',
      'browser/downgrade/user_data_downgrade_browsertest.cc',
      'test/data/resource.rc',
    ],
    # Cross-platform (except Mac) views interactive tests.
    # TODO(tapted): Move these to chrome_interactive_ui_test_views_sources when
    # the corresponding files are moved in chrome_browser_ui.gypi (i.e. out of
    # chrome_browser_ui_views_non_mac_sources).
    'chrome_interactive_ui_test_views_non_mac_sources': [
      'browser/ui/views/bookmarks/bookmark_bar_view_test.cc',
      'browser/ui/views/bookmarks/bookmark_bar_view_test_helper.h',
      'browser/ui/views/certificate_selector_browsertest.cc',
      'browser/ui/views/constrained_window_views_browsertest.cc',
      'browser/ui/views/extensions/extension_dialog_interactive_uitest.cc',
      'browser/ui/views/find_bar_views_interactive_uitest.cc',
      'browser/ui/views/frame/browser_view_focus_uitest.cc',
      'browser/ui/views/frame/browser_view_interactive_uitest.cc',
      'browser/ui/views/keyboard_access_browsertest.cc',
      'browser/ui/views/location_bar/location_icon_view_interactive_uitest.cc',
      'browser/ui/views/location_bar/page_action_image_view_interactive_uitest.cc',
      'browser/ui/views/location_bar/star_view_browsertest.cc',
      'browser/ui/views/omnibox/omnibox_view_views_browsertest.cc',
      'browser/ui/views/passwords/manage_passwords_bubble_view_interactive_uitest.cc',
      'browser/ui/views/passwords/manage_passwords_icon_view_interactive_uitest.cc',
      'browser/ui/views/ssl_client_certificate_selector_browsertest.cc',
      'browser/ui/views/tabs/tab_drag_controller_interactive_uitest.cc',
      'browser/ui/views/tabs/tab_drag_controller_interactive_uitest.h',
      'browser/ui/views/toolbar/toolbar_action_view_interactive_uitest.cc',
      'browser/ui/views/toolbar/toolbar_button_interactive_uitest.cc',
      'browser/ui/views/toolbar/toolbar_view_interactive_uitest.cc',
      'browser/ui/views/translate/translate_bubble_test_utils_views.cc',
      'test/base/interactive_test_utils_views.cc',
    ],
    'chrome_interactive_ui_test_notifications_sources': [
      'browser/extensions/api/notifications/notifications_apitest.cc',
      'browser/notifications/message_center_notifications_browsertest.cc',
    ],
    'chrome_interactive_ui_test_chromeos_sources': [
      '../ash/accelerators/accelerator_interactive_uitest_chromeos.cc',
      'browser/chromeos/accessibility/accessibility_highlight_manager_interactive_uitest.cc',
      'browser/chromeos/accessibility/magnification_controller_browsertest.cc',
      'browser/chromeos/accessibility/speech_monitor.cc',
      'browser/chromeos/accessibility/speech_monitor.h',
      'browser/chromeos/accessibility/spoken_feedback_browsertest.cc',
      'browser/chromeos/accessibility/sticky_keys_browsertest.cc',
      'browser/chromeos/input_method/textinput_browsertest.cc',
      'browser/chromeos/input_method/textinput_surroundingtext_browsertest.cc',
      'browser/chromeos/input_method/textinput_test_helper.cc',
      'browser/chromeos/input_method/textinput_test_helper.h',
      'browser/chromeos/login/eula_browsertest.cc',
      'browser/chromeos/login/lock/screen_locker_browsertest.cc',
      'browser/chromeos/login/lock/screen_locker_tester.cc',
      'browser/chromeos/login/lock/screen_locker_tester.h',
      'browser/chromeos/login/login_browsertest.cc',
      'browser/chromeos/login/login_manager_test.cc',
      'browser/chromeos/login/login_manager_test.h',
      'browser/chromeos/login/login_ui_browsertest.cc',
      'browser/chromeos/login/mixin_based_browser_test.cc',
      'browser/chromeos/login/mixin_based_browser_test.h',
      'browser/chromeos/login/oobe_browsertest.cc',
      'browser/chromeos/login/screenshot_testing/SkDiffPixelsMetric.h',
      'browser/chromeos/login/screenshot_testing/SkDiffPixelsMetric_cpu.cpp',
      'browser/chromeos/login/screenshot_testing/SkImageDiffer.cpp',
      'browser/chromeos/login/screenshot_testing/SkImageDiffer.h',
      'browser/chromeos/login/screenshot_testing/SkPMetric.cpp',
      'browser/chromeos/login/screenshot_testing/SkPMetric.h',
      'browser/chromeos/login/screenshot_testing/screenshot_tester.cc',
      'browser/chromeos/login/screenshot_testing/screenshot_tester.h',
      'browser/chromeos/login/screenshot_testing/screenshot_tester.h',
      'browser/chromeos/login/screenshot_testing/screenshot_testing_mixin.cc',
      'browser/chromeos/login/screenshot_testing/screenshot_testing_mixin.h',
      'browser/chromeos/login/test/https_forwarder.cc',
      'browser/chromeos/login/test/https_forwarder.h',
      'browser/chromeos/login/test/oobe_base_test.cc',
      'browser/chromeos/login/test/oobe_base_test.h',
      'browser/chromeos/login/users/wallpaper/wallpaper_manager_browsertest.cc',
      'browser/chromeos/login/users/wallpaper/wallpaper_manager_test_utils.cc',
      'browser/chromeos/login/users/wallpaper/wallpaper_manager_test_utils.h',
      'test/data/chromeos/service_login.html',
    ],
    'chrome_interactive_ui_test_ash_sources': [
      '../ash/drag_drop/drag_drop_interactive_uitest.cc',
      '../ash/wm/ash_native_cursor_manager_interactive_uitest.cc',
      'browser/ui/views/ash/tab_scrubber_browsertest.cc',
      'browser/ui/window_sizer/window_sizer_ash_uitest.cc',
    ],
    'chrome_interactive_ui_test_non_desktop_linux_sources': [
      # TODO(port): Everything here times out. Attempts have been made to fix
      # the individual failures, but each time I disable a test from these
      # suites, it seems like one or another starts timing out too.
      'browser/extensions/api/tabs/tabs_interactive_test.cc',
    ],
    'chrome_interactive_ui_test_non_linux_and_chromeos_sources': [
      # TODO(port): Disable all the interactive panel tests on all linux
      # platforms. These are badly busted on linux_aura, also time out
      # when run under openbox. ChromeOS doesn't use this panels code.
      'browser/ui/panels/panel_drag_browsertest.cc',
      'browser/ui/panels/stacked_panel_browsertest.cc',
    ],
    'chrome_automation_client_lib_sources': [
      'test/chromedriver/chrome/adb.h',
      'test/chromedriver/chrome/adb_impl.cc',
      'test/chromedriver/chrome/adb_impl.h',
      'test/chromedriver/chrome/automation_extension.cc',
      'test/chromedriver/chrome/automation_extension.h',
      'test/chromedriver/chrome/chrome.h',
      'test/chromedriver/chrome/chrome_android_impl.cc',
      'test/chromedriver/chrome/chrome_android_impl.h',
      'test/chromedriver/chrome/chrome_desktop_impl.cc',
      'test/chromedriver/chrome/chrome_desktop_impl.h',
      'test/chromedriver/chrome/chrome_finder.cc',
      'test/chromedriver/chrome/chrome_finder.h',
      'test/chromedriver/chrome/chrome_finder_mac.mm',
      'test/chromedriver/chrome/chrome_impl.cc',
      'test/chromedriver/chrome/chrome_impl.h',
      'test/chromedriver/chrome/chrome_remote_impl.cc',
      'test/chromedriver/chrome/chrome_remote_impl.h',
      'test/chromedriver/chrome/console_logger.cc',
      'test/chromedriver/chrome/console_logger.h',
      'test/chromedriver/chrome/debugger_tracker.cc',
      'test/chromedriver/chrome/debugger_tracker.h',
      'test/chromedriver/chrome/device_manager.cc',
      'test/chromedriver/chrome/device_manager.h',
      'test/chromedriver/chrome/device_metrics.cc',
      'test/chromedriver/chrome/device_metrics.h',
      'test/chromedriver/chrome/devtools_client.h',
      'test/chromedriver/chrome/devtools_client_impl.cc',
      'test/chromedriver/chrome/devtools_client_impl.h',
      'test/chromedriver/chrome/devtools_event_listener.cc',
      'test/chromedriver/chrome/devtools_event_listener.h',
      'test/chromedriver/chrome/devtools_http_client.cc',
      'test/chromedriver/chrome/devtools_http_client.h',
      'test/chromedriver/chrome/dom_tracker.cc',
      'test/chromedriver/chrome/dom_tracker.h',
      'test/chromedriver/chrome/frame_tracker.cc',
      'test/chromedriver/chrome/frame_tracker.h',
      'test/chromedriver/chrome/geolocation_override_manager.cc',
      'test/chromedriver/chrome/geolocation_override_manager.h',
      'test/chromedriver/chrome/geoposition.h',
      'test/chromedriver/chrome/heap_snapshot_taker.cc',
      'test/chromedriver/chrome/heap_snapshot_taker.h',
      'test/chromedriver/chrome/javascript_dialog_manager.cc',
      'test/chromedriver/chrome/javascript_dialog_manager.h',
      'test/chromedriver/chrome/log.cc',
      'test/chromedriver/chrome/log.h',
      'test/chromedriver/chrome/mobile_device.cc',
      'test/chromedriver/chrome/mobile_device.h',
      'test/chromedriver/chrome/mobile_device_list.cc',
      'test/chromedriver/chrome/mobile_device_list.h',
      'test/chromedriver/chrome/mobile_emulation_override_manager.cc',
      'test/chromedriver/chrome/mobile_emulation_override_manager.h',
      'test/chromedriver/chrome/navigation_tracker.cc',
      'test/chromedriver/chrome/navigation_tracker.h',
      'test/chromedriver/chrome/network_conditions.cc',
      'test/chromedriver/chrome/network_conditions.h',
      'test/chromedriver/chrome/network_conditions_override_manager.cc',
      'test/chromedriver/chrome/network_conditions_override_manager.h',
      'test/chromedriver/chrome/network_list.cc',
      'test/chromedriver/chrome/network_list.h',
      'test/chromedriver/chrome/status.cc',
      'test/chromedriver/chrome/status.h',
      'test/chromedriver/chrome/ui_events.cc',
      'test/chromedriver/chrome/ui_events.h',
      'test/chromedriver/chrome/util.cc',
      'test/chromedriver/chrome/util.h',
      'test/chromedriver/chrome/version.cc',
      'test/chromedriver/chrome/version.h',
      'test/chromedriver/chrome/web_view.h',
      'test/chromedriver/chrome/web_view_impl.cc',
      'test/chromedriver/chrome/web_view_impl.h',
      'test/chromedriver/net/adb_client_socket.cc',
      'test/chromedriver/net/adb_client_socket.h',
      'test/chromedriver/net/net_util.cc',
      'test/chromedriver/net/net_util.h',
      'test/chromedriver/net/port_server.cc',
      'test/chromedriver/net/port_server.h',
      'test/chromedriver/net/sync_websocket.h',
      'test/chromedriver/net/sync_websocket_factory.cc',
      'test/chromedriver/net/sync_websocket_factory.h',
      'test/chromedriver/net/sync_websocket_impl.cc',
      'test/chromedriver/net/sync_websocket_impl.h',
      'test/chromedriver/net/timeout.cc',
      'test/chromedriver/net/timeout.h',
      'test/chromedriver/net/url_request_context_getter.cc',
      'test/chromedriver/net/url_request_context_getter.h',
      'test/chromedriver/net/websocket.cc',
      'test/chromedriver/net/websocket.h',
    ],
    'performance_browser_tests_sources': [
      'app/chrome_command_ids.h',
      'app/chrome_dll.rc',
      'app/chrome_dll_resource.h',
      'app/chrome_version.rc.version',
      'browser/extensions/api/cast_streaming/performance_test.cc',
      'browser/extensions/api/tab_capture/tab_capture_performancetest.cc',
      'browser/extensions/browsertest_util.cc',
      'browser/extensions/extension_apitest.cc',
      'browser/extensions/extension_browsertest.cc',
      'browser/extensions/extension_test_notification_observer.cc',
      'browser/extensions/updater/extension_cache_fake.cc',
      'browser/extensions/updater/extension_cache_fake.h',
      'test/base/browser_perf_tests_main.cc',
      'test/base/chrome_render_view_test.cc',
      'test/base/chrome_render_view_test.h',
      'test/perf/mach_ports_performancetest.cc',
      'test/perf/url_parse_perftest.cc',
    ],
    'chrome_driver_lib_sources': [
      '../third_party/webdriver/atoms.cc',
      '../third_party/webdriver/atoms.h',
      'common/chrome_constants.cc',
      'common/chrome_constants.h',
      'test/chromedriver/alert_commands.cc',
      'test/chromedriver/alert_commands.h',
      'test/chromedriver/basic_types.cc',
      'test/chromedriver/basic_types.h',
      'test/chromedriver/capabilities.cc',
      'test/chromedriver/capabilities.h',
      'test/chromedriver/chrome/browser_info.cc',
      'test/chromedriver/chrome/browser_info.h',
      'test/chromedriver/chrome_launcher.cc',
      'test/chromedriver/chrome_launcher.h',
      'test/chromedriver/command.h',
      'test/chromedriver/command_listener.h',
      'test/chromedriver/command_listener_proxy.cc',
      'test/chromedriver/command_listener_proxy.h',
      'test/chromedriver/commands.cc',
      'test/chromedriver/commands.h',
      'test/chromedriver/element_commands.cc',
      'test/chromedriver/element_commands.h',
      'test/chromedriver/element_util.cc',
      'test/chromedriver/element_util.h',
      'test/chromedriver/key_converter.cc',
      'test/chromedriver/key_converter.h',
      'test/chromedriver/keycode_text_conversion.h',
      'test/chromedriver/keycode_text_conversion_mac.mm',
      'test/chromedriver/keycode_text_conversion_ozone.cc',
      'test/chromedriver/keycode_text_conversion_win.cc',
      'test/chromedriver/keycode_text_conversion_x.cc',
      'test/chromedriver/logging.cc',
      'test/chromedriver/logging.h',
      'test/chromedriver/performance_logger.cc',
      'test/chromedriver/performance_logger.h',
      'test/chromedriver/server/http_handler.cc',
      'test/chromedriver/server/http_handler.h',
      'test/chromedriver/session.cc',
      'test/chromedriver/session.h',
      'test/chromedriver/session_commands.cc',
      'test/chromedriver/session_commands.h',
      'test/chromedriver/session_thread_map.h',
      'test/chromedriver/util.cc',
      'test/chromedriver/util.h',
      'test/chromedriver/window_commands.cc',
      'test/chromedriver/window_commands.h',
    ],
    'chrome_driver_unittests_sources': [
      'test/chromedriver/capabilities_unittest.cc',
      'test/chromedriver/chrome/browser_info_unittest.cc',
      'test/chromedriver/chrome/chrome_finder_unittest.cc',
      'test/chromedriver/chrome/console_logger_unittest.cc',
      'test/chromedriver/chrome/device_manager_unittest.cc',
      'test/chromedriver/chrome/devtools_client_impl_unittest.cc',
      'test/chromedriver/chrome/devtools_http_client_unittest.cc',
      'test/chromedriver/chrome/dom_tracker_unittest.cc',
      'test/chromedriver/chrome/frame_tracker_unittest.cc',
      'test/chromedriver/chrome/geolocation_override_manager_unittest.cc',
      'test/chromedriver/chrome/heap_snapshot_taker_unittest.cc',
      'test/chromedriver/chrome/javascript_dialog_manager_unittest.cc',
      'test/chromedriver/chrome/mobile_emulation_override_manager_unittest.cc',
      'test/chromedriver/chrome/navigation_tracker_unittest.cc',
      'test/chromedriver/chrome/network_conditions_override_manager_unittest.cc',
      'test/chromedriver/chrome/recorder_devtools_client.cc',
      'test/chromedriver/chrome/recorder_devtools_client.h',
      'test/chromedriver/chrome/status_unittest.cc',
      'test/chromedriver/chrome/stub_chrome.cc',
      'test/chromedriver/chrome/stub_chrome.h',
      'test/chromedriver/chrome/stub_devtools_client.cc',
      'test/chromedriver/chrome/stub_devtools_client.h',
      'test/chromedriver/chrome/stub_web_view.cc',
      'test/chromedriver/chrome/stub_web_view.h',
      'test/chromedriver/chrome/web_view_impl_unittest.cc',
      'test/chromedriver/chrome_launcher_unittest.cc',
      'test/chromedriver/command_listener_proxy_unittest.cc',
      'test/chromedriver/commands_unittest.cc',
      'test/chromedriver/logging_unittest.cc',
      'test/chromedriver/net/timeout_unittest.cc',
      'test/chromedriver/performance_logger_unittest.cc',
      'test/chromedriver/server/http_handler_unittest.cc',
      'test/chromedriver/session_commands_unittest.cc',
      'test/chromedriver/session_unittest.cc',
      'test/chromedriver/util_unittest.cc',
    ],
    'chrome_driver_tests_sources': [
      'test/chromedriver/key_converter_unittest.cc',
      'test/chromedriver/keycode_text_conversion_unittest.cc',
      'test/chromedriver/net/net_util_unittest.cc',
      'test/chromedriver/net/port_server_unittest.cc',
      'test/chromedriver/net/sync_websocket_impl_unittest.cc',
      'test/chromedriver/net/test_http_server.cc',
      'test/chromedriver/net/test_http_server.h',
      'test/chromedriver/net/websocket_unittest.cc',
      'test/chromedriver/test_util.cc',
      'test/chromedriver/test_util.h',
    ],
    'sync_integration_tests_sources': [
      'app/chrome_command_ids.h',
      'app/chrome_dll.rc',
      'app/chrome_dll_resource.h',
      'app/chrome_version.rc.version',
      'browser/sync/test/integration/enable_disable_test.cc',
      'browser/sync/test/integration/migration_test.cc',
      'browser/sync/test/integration/single_client_app_list_sync_test.cc',
      'browser/sync/test/integration/single_client_apps_sync_test.cc',
      'browser/sync/test/integration/single_client_arc_package_sync_test.cc',
      'browser/sync/test/integration/single_client_bookmarks_sync_test.cc',
      'browser/sync/test/integration/single_client_dictionary_sync_test.cc',
      'browser/sync/test/integration/single_client_directory_sync_test.cc',
      'browser/sync/test/integration/single_client_extensions_sync_test.cc',
      'browser/sync/test/integration/single_client_password_manager_setting_migrator_service_sync_test.cc',
      'browser/sync/test/integration/single_client_passwords_sync_test.cc',
      'browser/sync/test/integration/single_client_preferences_sync_test.cc',
      'browser/sync/test/integration/single_client_search_engines_sync_test.cc',
      'browser/sync/test/integration/single_client_sessions_sync_test.cc',
      'browser/sync/test/integration/single_client_supervised_user_settings_sync_test.cc',
      'browser/sync/test/integration/single_client_themes_sync_test.cc',
      'browser/sync/test/integration/single_client_typed_urls_sync_test.cc',
      'browser/sync/test/integration/single_client_wallet_sync_test.cc',
      'browser/sync/test/integration/single_client_wifi_credentials_sync_test.cc',
      'browser/sync/test/integration/sync_auth_test.cc',
      'browser/sync/test/integration/sync_errors_test.cc',
      'browser/sync/test/integration/sync_exponential_backoff_test.cc',
      'browser/sync/test/integration/two_client_app_list_sync_test.cc',
      'browser/sync/test/integration/two_client_apps_sync_test.cc',
      'browser/sync/test/integration/two_client_arc_package_sync_test.cc',
      'browser/sync/test/integration/two_client_autofill_sync_test.cc',
      'browser/sync/test/integration/two_client_bookmarks_sync_test.cc',
      'browser/sync/test/integration/two_client_dictionary_sync_test.cc',
      'browser/sync/test/integration/two_client_extension_settings_and_app_settings_sync_test.cc',
      'browser/sync/test/integration/two_client_extensions_sync_test.cc',
      'browser/sync/test/integration/two_client_password_manager_setting_migrator_service_sync_test.cc',
      'browser/sync/test/integration/two_client_passwords_sync_test.cc',
      'browser/sync/test/integration/two_client_preferences_sync_test.cc',
      'browser/sync/test/integration/two_client_search_engines_sync_test.cc',
      'browser/sync/test/integration/two_client_sessions_sync_test.cc',
      'browser/sync/test/integration/two_client_themes_sync_test.cc',
      'browser/sync/test/integration/two_client_typed_urls_sync_test.cc',
      'browser/sync/test/integration/two_client_wifi_credentials_sync_test.cc',
      'test/base/browser_tests_main.cc',
      'test/data/resource.rc',
    ],
    'test_support_sync_integration_sources': [
      'browser/sync/test/integration/apps_helper.cc',
      'browser/sync/test/integration/apps_helper.h',
      'browser/sync/test/integration/autofill_helper.cc',
      'browser/sync/test/integration/autofill_helper.h',
      'browser/sync/test/integration/await_match_status_change_checker.cc',
      'browser/sync/test/integration/await_match_status_change_checker.h',
      'browser/sync/test/integration/bookmarks_helper.cc',
      'browser/sync/test/integration/bookmarks_helper.h',
      'browser/sync/test/integration/dictionary_helper.cc',
      'browser/sync/test/integration/dictionary_helper.h',
      'browser/sync/test/integration/dictionary_load_observer.cc',
      'browser/sync/test/integration/dictionary_load_observer.h',
      'browser/sync/test/integration/extension_settings_helper.cc',
      'browser/sync/test/integration/extension_settings_helper.h',
      'browser/sync/test/integration/extensions_helper.cc',
      'browser/sync/test/integration/extensions_helper.h',
      'browser/sync/test/integration/fake_server_invalidation_service.cc',
      'browser/sync/test/integration/fake_server_invalidation_service.h',
      'browser/sync/test/integration/fake_server_match_status_checker.cc',
      'browser/sync/test/integration/fake_server_match_status_checker.h',
      'browser/sync/test/integration/migration_waiter.cc',
      'browser/sync/test/integration/migration_waiter.h',
      'browser/sync/test/integration/migration_watcher.cc',
      'browser/sync/test/integration/migration_watcher.h',
      'browser/sync/test/integration/multi_client_status_change_checker.cc',
      'browser/sync/test/integration/multi_client_status_change_checker.h',
      'browser/sync/test/integration/p2p_invalidation_forwarder.cc',
      'browser/sync/test/integration/p2p_invalidation_forwarder.h',
      'browser/sync/test/integration/p2p_sync_refresher.cc',
      'browser/sync/test/integration/p2p_sync_refresher.h',
      'browser/sync/test/integration/password_manager_setting_migrator_helper.cc',
      'browser/sync/test/integration/password_manager_setting_migrator_helper.h',
      'browser/sync/test/integration/passwords_helper.cc',
      'browser/sync/test/integration/passwords_helper.h',
      'browser/sync/test/integration/preferences_helper.cc',
      'browser/sync/test/integration/preferences_helper.h',
      'browser/sync/test/integration/profile_sync_service_harness.cc',
      'browser/sync/test/integration/profile_sync_service_harness.h',
      'browser/sync/test/integration/quiesce_status_change_checker.cc',
      'browser/sync/test/integration/quiesce_status_change_checker.h',
      'browser/sync/test/integration/retry_verifier.cc',
      'browser/sync/test/integration/retry_verifier.h',
      'browser/sync/test/integration/search_engines_helper.cc',
      'browser/sync/test/integration/search_engines_helper.h',
      'browser/sync/test/integration/sessions_helper.cc',
      'browser/sync/test/integration/sessions_helper.h',
      'browser/sync/test/integration/single_client_status_change_checker.cc',
      'browser/sync/test/integration/single_client_status_change_checker.h',
      'browser/sync/test/integration/status_change_checker.cc',
      'browser/sync/test/integration/status_change_checker.h',
      'browser/sync/test/integration/sync_app_helper.cc',
      'browser/sync/test/integration/sync_app_helper.h',
      'browser/sync/test/integration/sync_app_list_helper.cc',
      'browser/sync/test/integration/sync_app_list_helper.h',
      'browser/sync/test/integration/sync_arc_package_helper.cc',
      'browser/sync/test/integration/sync_arc_package_helper.h',
      'browser/sync/test/integration/sync_datatype_helper.cc',
      'browser/sync/test/integration/sync_datatype_helper.h',
      'browser/sync/test/integration/sync_extension_helper.cc',
      'browser/sync/test/integration/sync_extension_helper.h',
      'browser/sync/test/integration/sync_extension_installer.cc',
      'browser/sync/test/integration/sync_extension_installer.h',
      'browser/sync/test/integration/sync_integration_test_util.cc',
      'browser/sync/test/integration/sync_integration_test_util.h',
      'browser/sync/test/integration/sync_test.cc',
      'browser/sync/test/integration/sync_test.h',
      'browser/sync/test/integration/themes_helper.cc',
      'browser/sync/test/integration/themes_helper.h',
      'browser/sync/test/integration/typed_urls_helper.cc',
      'browser/sync/test/integration/typed_urls_helper.h',
      'browser/sync/test/integration/updated_progress_marker_checker.cc',
      'browser/sync/test/integration/updated_progress_marker_checker.h',
      'browser/sync/test/integration/wifi_credentials_helper.cc',
      'browser/sync/test/integration/wifi_credentials_helper.h',
      'browser/sync/test/integration/wifi_credentials_helper_chromeos.cc',
      'browser/sync/test/integration/wifi_credentials_helper_chromeos.h',
    ],
    'sync_performance_tests_sources': [
      'app/chrome_command_ids.h',
      'app/chrome_dll.rc',
      'app/chrome_dll_resource.h',
      'app/chrome_version.rc.version',
      'browser/sync/test/integration/performance/autofill_sync_perf_test.cc',
      'browser/sync/test/integration/performance/bookmarks_sync_perf_test.cc',
      'browser/sync/test/integration/performance/dictionary_sync_perf_test.cc',
      'browser/sync/test/integration/performance/extensions_sync_perf_test.cc',
      'browser/sync/test/integration/performance/passwords_sync_perf_test.cc',
      'browser/sync/test/integration/performance/sessions_sync_perf_test.cc',
      'browser/sync/test/integration/performance/sync_timing_helper.cc',
      'browser/sync/test/integration/performance/sync_timing_helper.h',
      'browser/sync/test/integration/performance/typed_urls_sync_perf_test.cc',
      'test/base/browser_perf_tests_main.cc',
      'test/data/resource.rc',
    ],
  },
  'includes': [
    'js_unittest_vars.gypi',
  ],
  'targets': [
    {
      # This target contains non-unittest test utilities that don't belong in
      # production libraries but are used by more than one test executable.
      #
      # GN version: //chrome/test:test_support_ui
      'target_name': 'test_support_ui',
      'type': 'static_library',
      'dependencies': [
        '../components/components.gyp:metrics_test_support',
        '../components/components.gyp:os_crypt_test_support',
        '../components/components.gyp:password_manager_core_browser_test_support',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        'common',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'browser/password_manager/password_manager_test_base.cc',
        'browser/password_manager/password_manager_test_base.h',
        'browser/signin/token_revoker_test_utils.cc',
        'browser/signin/token_revoker_test_utils.h',
        'browser/ui/webui/signin/login_ui_test_utils.cc',
        'browser/ui/webui/signin/login_ui_test_utils.h',
        'test/base/in_process_browser_test.cc',
        'test/base/in_process_browser_test.h',
        'test/base/in_process_browser_test_mac.cc',
        'test/base/ui_test_utils.cc',
        'test/base/ui_test_utils.h',
      ],
      'conditions': [
        ['enable_plugins==1', {
          "sources" : [
            'test/ppapi/ppapi_test.cc',
            'test/ppapi/ppapi_test.h',
          ],
        }],
      ],
    },
    {
      # GN version: //chrome/test:interactive_ui_tests
      'target_name': 'interactive_ui_tests',
      'type': 'executable',
      'dependencies': [
        'browser',
        'chrome_features.gyp:chrome_common_features',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'chrome_resources.gyp:packed_extra_resources',
        'chrome_resources.gyp:packed_resources',
        'debugger',
        'renderer',
        'test_support_common',
        'test_support_ui',
        '../components/components.gyp:guest_view_test_support',
        '../components/components_resources.gyp:components_resources',
        '../content/app/resources/content_resources.gyp:content_resources',
        '../content/content_shell_and_tests.gyp:content_browser_test_base',
        '../crypto/crypto.gyp:crypto_test_support',
        '../google_apis/google_apis.gyp:google_apis_test_support',
        '../net/net.gyp:net',
        '../net/net.gyp:net_resources',
        '../net/net.gyp:net_test_support',
        '../ppapi/ppapi_internal.gyp:ppapi_tests',
        '../skia/skia.gyp:skia',
        '../sync/sync.gyp:sync',
	'../sync/sync.gyp:test_support_sync_api',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/hunspell/hunspell.gyp:hunspell',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libpng/libpng.gyp:libpng',
        '../third_party/zlib/zlib.gyp:zlib',
        '../ui/base/ui_base.gyp:ui_base_test_support',
        '../ui/resources/ui_resources.gyp:ui_test_pak',
        '../ui/web_dialogs/web_dialogs.gyp:web_dialogs_test_support',
        # Runtime dependencies
        '../third_party/mesa/mesa.gyp:osmesa',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
      ],
      'sources': [
        '<@(chrome_interactive_ui_test_extensions_sources)',
        '<@(chrome_interactive_ui_test_sources)',
      ],
      'conditions': [
        ['use_x11==1', {
          'dependencies': [
            '../build/linux/system.gyp:xtst',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:nss',
          ],
        }, {  # Non-Linux platforms (Linux includes ChromeOS here).
          'sources': [
            '<@(chrome_interactive_ui_test_non_linux_and_chromeos_sources)',
          ],
        }],
        ['OS=="linux" and chromeos==0', {
          'sources!': [
            # TODO(port): This times out. Attempts have been made to fix the
            # individual failures, but each time I disable a test from these
            # suites, it seems like one or another starts timing out too.

            # Note: list duplicated in GN build.
            'browser/ui/views/keyboard_access_browsertest.cc',
          ],
        }, {  # Everything but desktop Linux.
          'sources': [ '<@(chrome_interactive_ui_test_non_desktop_linux_sources)' ],
        }],
        ['OS=="linux" and chromeos==0 and use_ozone==0', {
          'sources': [ 'browser/ui/libgtk2ui/select_file_dialog_interactive_uitest.cc' ],
          'dependencies': [ '../build/linux/system.gyp:gtk2' ],
        }],
        ['use_ash==1', {
          'sources': [ '<@(chrome_interactive_ui_test_ash_sources)' ],
          'dependencies': [
            '../ash/ash.gyp:ash_interactive_ui_test_support',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            'chrome.gyp:install_static_util',
          ],
        }],
        ['OS=="mac"', {
          'dependencies': [
            'chrome'
          ],
          # See comment about the same line in chrome/chrome_tests.gypi.
          'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
          'conditions' : [
            # The browser window can be views or Cocoa on Mac. Test accordingly.
            ['mac_views_browser==1', {
              'sources': [ '<@(chrome_interactive_ui_test_views_non_mac_sources)' ],
              # Following tests needs a refactoring to works with mac_views.
              'sources!': [
                # Aura depended tests.
                'browser/ui/views/bookmarks/bookmark_bar_view_test.cc',
              ]
            }, {
              'sources': [ '<@(chrome_interactive_ui_test_cocoa_sources)' ],
            }],  # mac_views_browser==1
          ],
        }, {  # Non-Mac.
          'sources': [ '<@(chrome_interactive_ui_test_views_non_mac_sources)' ],
        }],
        ['notifications == 1', {
          # Common notifications tests.
          'sources': [
            '<@(chrome_interactive_ui_test_notifications_sources)',
          ],
          'conditions': [
            ['chromeos == 1', {
              'sources': [
                'browser/notifications/login_state_notification_blocker_chromeos_browsertest.cc',
              ],
            }, {
              # Non-ChromeOS notifications tests (ChromeOS does not use
              # cross-platform panels).
              'sources': [
                # Note: List duplicated in GN build.
                'browser/notifications/notification_interactive_uitest.cc',
                'browser/notifications/platform_notification_service_interactive_uitest.cc',
              ],
            }],
            ['OS=="android"', {
              'sources!': [
                # Note: List duplicated in GN build.

                # Android does not use the message center-based Notification system.
                'browser/notifications/message_center_notifications_browsertest.cc',

                # TODO(peter): Enable the Notification browser tests.
                'browser/notifications/notification_ui_browsertest.cc',
                'browser/notifications/platform_notification_service_ui_browsertest.cc',
              ]
            }],
          ],
        }],
        ['toolkit_views==1', {
          'sources': [ '<@(chrome_interactive_ui_test_views_sources)' ],
          'dependencies': [
            '../ui/views/controls/webview/webview_tests.gyp:webview_test_support',
            '../ui/views/views.gyp:views',
            '../ui/views/views.gyp:views_test_support',
          ],
          'conditions': [
            ['use_aura==1', {
              'dependencies': [
                '../ui/touch_selection/ui_touch_selection.gyp:ui_touch_selection',
              ],
            }],
          ],
        }],
        ['use_aura==0 or chromeos==1', {
          'sources!': [
            '../ui/views/corewm/desktop_capture_controller_unittest.cc',
          ],
        }],
        ['chromeos==1', {
          'dependencies': [
            '../ash/ash_resources.gyp:ash_resources',
            '../chromeos/chromeos.gyp:chromeos',
          ],
          'conditions': [
            ['disable_nacl==0 and disable_nacl_untrusted==0', {
              'dependencies': [
                '../native_client/src/trusted/service_runtime/linux/nacl_bootstrap.gyp:nacl_helper_bootstrap',
                '../components/nacl.gyp:nacl_helper',
                '../components/nacl_nonsfi.gyp:nacl_helper_nonsfi',
              ],
            }],
          ],
          'sources': [
            '<@(chrome_interactive_ui_test_chromeos_sources)',
          ],
          'sources!': [
            # Note: List duplicated in GN build.
            '../ui/views/widget/desktop_aura/desktop_window_tree_host_x11_interactive_uitest.cc',
            '../ui/views/widget/desktop_aura/x11_topmost_window_finder_interactive_uitest.cc',

            # Use only the _chromeos version on ChromeOS.
            'test/base/view_event_test_platform_part_default.cc',
          ],
        }, {  # Non-ChromeOS.
          # ChromeOS doesn't use panels, everybody else does.
          'sources': [ '<@(chrome_interactive_ui_test_panels_sources)' ],
          'conditions': [
            # TODO(tapted): Include on mac when views panels are ported.
            [ 'toolkit_views==1 and OS != "mac"', {
              'sources': [
                '<@(chrome_interactive_ui_test_panels_views_sources)',
              ],
            }]
          ],
        }],
        ['chromeos==1 and branding=="Chrome"', {
          'sources!': [
            # These tests are failing on official cros bots. crbug.com/431450.
            # Note: list duplicated in GN build.
            'browser/ui/views/bookmarks/bookmark_bar_view_test.cc',
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            '../third_party/wtl/include',
          ],
          'dependencies': [
            '../third_party/isimpledom/isimpledom.gyp:isimpledom',
            '../ui/resources/ui_resources.gyp:ui_resources',
            'chrome.gyp:chrome_version_resources',
          ],
          'sources': [
            '<@(chrome_interactive_ui_test_win_sources)',

            # TODO:  It would be nice to have these pulled in
            # automatically from direct_dependent_settings in
            # their various targets (net.gyp:net_resources, etc.),
            # but that causes errors in other targets when
            # resulting .res files get referenced multiple times.
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_unscaled_resources.rc',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'conditions': [
                ['incremental_chrome_dll==1', {
                  'UseLibraryDependencyInputs': "true",
                }],
              ],
            },
          },
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        }],  # OS != "win"
        ['enable_app_list==1', {
          'sources': [ '<@(chrome_interactive_ui_test_app_list_sources)' ],
        }],
      ],  # conditions
    },
    {
      # GN version: //chrome/test/chromedriver:automation_client_lib
      'target_name': 'automation_client_lib',
      'type': 'static_library',
      'hard_dependency': 1,
      'dependencies': [
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../net/net.gyp:net',
        '../third_party/zlib/zlib.gyp:minizip',
        '../third_party/zlib/zlib.gyp:zlib',
        '../ui/accessibility/accessibility.gyp:ax_gen',
        '../ui/base/ui_base.gyp:ui_base',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
      'sources': [
        '<@(chrome_automation_client_lib_sources)',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/embedded_automation_extension.cc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/embedded_automation_extension.h',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/js.cc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/js.h',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/user_data_dir.cc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/user_data_dir.h',
      ],
      'actions': [
        {
          # GN version: //chrome/test/chromedriver:embed_js_in_cpp
          'action_name': 'embed_js_in_cpp',
          'inputs': [
            'test/chromedriver/cpp_source.py',
            'test/chromedriver/embed_js_in_cpp.py',
            'test/chromedriver/js/add_cookie.js',
            'test/chromedriver/js/call_function.js',
            'test/chromedriver/js/execute_async_script.js',
            'test/chromedriver/js/focus.js',
            'test/chromedriver/js/get_element_region.js',
            'test/chromedriver/js/is_option_element_toggleable.js',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/js.cc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/js.h',
          ],
          'action': [ 'python',
                      'test/chromedriver/embed_js_in_cpp.py',
                      '--directory',
                      '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome',
                      'test/chromedriver/js/add_cookie.js',
                      'test/chromedriver/js/call_function.js',
                      'test/chromedriver/js/execute_async_script.js',
                      'test/chromedriver/js/focus.js',
                      'test/chromedriver/js/get_element_region.js',
                      'test/chromedriver/js/is_option_element_toggleable.js',
          ],
          'message': 'Generating sources for embedding js in chromedriver',
        },
        {
          # GN version: //chrome/test/chromedriver:embed_user_data_dir_in_cpp
          'action_name': 'embed_user_data_dir_in_cpp',
          'inputs': [
            'test/chromedriver/cpp_source.py',
            'test/chromedriver/embed_user_data_dir_in_cpp.py',
            'test/chromedriver/chrome/preferences.txt',
            'test/chromedriver/chrome/local_state.txt',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/user_data_dir.cc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/user_data_dir.h',
          ],
          'action': [ 'python',
                      'test/chromedriver/embed_user_data_dir_in_cpp.py',
                      '--directory',
                      '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome',
                      'test/chromedriver/chrome/preferences.txt',
                      'test/chromedriver/chrome/local_state.txt',
          ],
          'message': 'Generating sources for embedding user data dir in chromedriver',
        },
        {
          # GN version: //chrome/test/chromedriver:embed_extension_in_cpp
          'action_name': 'embed_extension_in_cpp',
          'inputs': [
            'test/chromedriver/cpp_source.py',
            'test/chromedriver/embed_extension_in_cpp.py',
            'test/chromedriver/extension/background.js',
            'test/chromedriver/extension/manifest.json',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/embedded_automation_extension.cc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/embedded_automation_extension.h',
          ],
          'action': [ 'python',
                      'test/chromedriver/embed_extension_in_cpp.py',
                      '--directory',
                      '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome',
                      'test/chromedriver/extension/background.js',
                      'test/chromedriver/extension/manifest.json',
          ],
          'message': 'Generating sources for embedding automation extension',
        },
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      # GN version: //chrome/test/chromedriver:lib
      'target_name': 'chromedriver_lib',
      'type': 'static_library',
      'hard_dependency': 1,
      'dependencies': [
        'automation_client_lib',
        'common_constants.gyp:version_header',
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../crypto/crypto.gyp:crypto',
        '../net/net.gyp:http_server',
        '../net/net.gyp:net',
        '../third_party/zlib/google/zip.gyp:zip',
        '../ui/base/ui_base.gyp:ui_base',
        '../ui/events/events.gyp:dom_keycode_converter',
        '../ui/events/events.gyp:events_base',
        '../ui/events/ozone/events_ozone.gyp:events_ozone_layout',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '<@(chrome_driver_lib_sources)',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/version.cc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/version.h',
      ],
      'actions': [
        {
          # GN version: //chrome/test/chromedriver:embed_version_in_cpp
          'action_name': 'embed_version_in_cpp',
          'inputs': [
            'test/chromedriver/cpp_source.py',
            'test/chromedriver/embed_version_in_cpp.py',
            'test/chromedriver/VERSION',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/version.cc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/version.h',
          ],
          'action': [ 'python',
                      'test/chromedriver/embed_version_in_cpp.py',
                      '--version-file',
                      'test/chromedriver/VERSION',
                      '--directory',
                      '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver',
          ],
          'message': 'Generating version info',
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
      'conditions': [
        ['use_x11==1', {
          'dependencies': [
            '../build/linux/system.gyp:x11',
            '../ui/events/keycodes/events_keycodes.gyp:keycodes_x11',
            '../ui/gfx/x/gfx_x11.gyp:gfx_x11',
          ]
        }]
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      # GN version: //chrome/test/chromedriver
      'target_name': 'chromedriver',
      'type': 'executable',
      'dependencies': [
        'chromedriver_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/chromedriver/server/chromedriver_server.cc',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      # GN version: //chrome/test/chromedriver:chromedriver_unittests
      'target_name': 'chromedriver_unittests',
      'type': 'executable',
      'dependencies': [
        'chromedriver_lib',
        '../base/base.gyp:base',
        '../base/base.gyp:run_all_unittests',
        '../net/net.gyp:http_server',
        '../net/net.gyp:net',
        '../testing/gtest.gyp:gtest',
        '../ui/base/ui_base.gyp:ui_base',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
      ],
      'include_dirs': [
        '..,'
      ],
      'sources': [
        '<@(chrome_driver_unittests_sources)',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    # ChromeDriver tests that aren't run on the main buildbot. Available
    # as an optional test type on trybots.
    {
      # GN version: //chrome/test/chromedriver:chromedriver_tests
      'target_name': 'chromedriver_tests',
      'type': 'executable',
      'dependencies': [
        'chromedriver_lib',
        '../base/base.gyp:base',
        '../base/base.gyp:run_all_unittests',
        '../net/net.gyp:http_server',
        '../net/net.gyp:net',
        '../testing/gtest.gyp:gtest',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..,'
      ],
      'sources': [
        '<@(chrome_driver_tests_sources)',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      # Executable that runs each browser test in a new process.
      'target_name': 'browser_tests',
      'type': 'executable',
      'dependencies': [
        'browser',
        'chrome_features.gyp:chrome_common_features',
        'chrome_resources.gyp:browser_tests_pak',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'chrome_resources.gyp:packed_extra_resources',
        'chrome_resources.gyp:packed_resources',
        'common/extensions/api/api.gyp:chrome_api',
        'renderer',
        'test_support_common',
        'test_support_sync_integration',
        'test_support_ui',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../components/components.gyp:autofill_content_risk_proto',
        '../components/components.gyp:autofill_content_test_support',
        '../components/components.gyp:captive_portal_test_support',
        '../components/components.gyp:certificate_reporting',
        '../components/components.gyp:dom_distiller_content_browser',
        '../components/components.gyp:dom_distiller_content_renderer',
        '../components/components.gyp:dom_distiller_test_support',
        '../components/components.gyp:guest_view_test_support',
        '../components/components.gyp:history_core_test_support',
        '../components/components.gyp:safe_browsing_db',
        '../components/components.gyp:ssl_config',
        '../components/components.gyp:subresource_filter_core_browser_test_support',
        '../components/components.gyp:subresource_filter_content_browser',
        '../components/components.gyp:test_database_manager',
        '../components/components.gyp:translate_core_common',
        '../components/components.gyp:zoom_test_support',
        '../components/components_resources.gyp:components_resources',
        '../components/components_strings.gyp:components_strings',
        '../content/content.gyp:common_features',
        '../content/content.gyp:feature_h264_with_openh264_ffmpeg',
        '../content/content_shell_and_tests.gyp:content_browser_test_base',
        '../crypto/crypto.gyp:crypto_test_support',
        '../device/bluetooth/bluetooth.gyp:device_bluetooth_mocks',
        '../device/serial/serial.gyp:device_serial_test_util',
        '../device/usb/usb.gyp:device_usb_mocks',
        '../extensions/common/api/api.gyp:extensions_api',
        '../google_apis/google_apis.gyp:google_apis_test_support',
        '../media/cast/cast.gyp:cast_test_utility',
        '../media/media.gyp:media',
        '../net/net.gyp:net',
        '../net/net.gyp:net_test_support',
        '../sdch/sdch.gyp:sdch',
        '../skia/skia.gyp:skia',
        '../sync/sync.gyp:sync',
        '../sync/sync.gyp:test_support_sync_api',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../testing/perf/perf_test.gyp:*',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        '../third_party/libaddressinput/libaddressinput.gyp:libaddressinput',
        '../third_party/webrtc/modules/modules.gyp:desktop_capture',
        '../third_party/widevine/cdm/widevine_cdm.gyp:widevine_cdm_version_h',
        '../ui/accessibility/accessibility.gyp:accessibility_test_support',
        '../ui/compositor/compositor.gyp:compositor_test_support',
        '../ui/resources/ui_resources.gyp:ui_resources',
        '../ui/web_dialogs/web_dialogs.gyp:web_dialogs_test_support',
        '../v8/src/v8.gyp:v8',
        # Runtime dependencies
        '../ppapi/ppapi_internal.gyp:power_saver_test_plugin',
        '../ppapi/ppapi_internal.gyp:ppapi_tests',
        '../remoting/remoting.gyp:remoting_browser_test_resources',
        '../remoting/remoting.gyp:remoting_webapp_unittests',
        '../third_party/mesa/mesa.gyp:osmesa',
        '../third_party/widevine/cdm/widevine_cdm.gyp:widevine_test_license_server',
      ],
      'include_dirs': [
        '..',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
      ],
      'sources': [
        '<@(chrome_browser_tests_extensions_sources)',
        '<@(chrome_browser_tests_sources)',
        '<@(chrome_browser_tests_webui_js_sources)',
        '<@(chrome_browser_extensions_test_support_sources)',
        'test/base/browser_tests_main.cc',
      ],
      'rules': [
        {
          'rule_name': 'js2webui',
          'extension': 'js',
          'msvs_external_rule': 1,
          'variables': {
            'conditions': [
              ['v8_use_external_startup_data==1', {
                'external_v8': 'y',
              }, {
                'external_v8': 'n',
              }],
            ],
          },
          'inputs': [
            '<(gypv8sh)',
            '<(PRODUCT_DIR)/v8_shell<(EXECUTABLE_SUFFIX)',
            '<(mock_js)',
            '<(test_api_js)',
            '<(js2gtest)',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/chrome/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT)-gen.cc',
            '<(PRODUCT_DIR)/test_data/chrome/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).js',
          ],
          'process_outputs_as_sources': 1,
          'action': [
            'python',
            '<@(_inputs)',
            '--external', '<(external_v8)',
            'webui',
            '<(RULE_INPUT_PATH)',
            'chrome/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).js',
            '<@(_outputs)',
          ],
        },
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'conditions': [
            ['incremental_chrome_dll==1', {
              'UseLibraryDependencyInputs': "true",
            }],
          ],
        },
      },
      'conditions': [
        ['chromeos==1', {
          'dependencies': [
            '../components/components.gyp:arc_test_support',
            '../third_party/boringssl/boringssl.gyp:boringssl',
          ]
        }, {
          'conditions': [
            ['OS == "linux" or OS == "win"', {
              'sources': [
                'browser/ui/views/ime/ime_warning_bubble_browsertest.cc',
                'browser/ui/views/ime/ime_window_browsertest.cc',
              ],
            }],
            ['OS=="win"', {
              'dependencies': [
                'chrome.gyp:install_static_util',
              ],
            }],
          ]
        }],
        ['enable_wifi_display==1', {
          'sources': [
            '<@(chrome_browser_tests_display_source_apitest)',
          ],
        }],
        ['enable_one_click_signin==0', {
          'sources!': [
            'browser/ui/sync/one_click_signin_links_delegate_impl_browsertest.cc',
          ]
        }],
        ['disable_nacl==0', {
          'sources':[
            'browser/extensions/extension_nacl_browsertest.cc',
            'browser/nacl_host/test/gdb_debug_stub_browsertest.cc',
          ],
          'dependencies': [
            'test/data/nacl/nacl_test_data.gyp:pnacl_url_loader_test',
          ],
          'conditions': [
            ['disable_nacl_untrusted==0', {
              'sources': [
                'test/nacl/nacl_browsertest.cc',
                'test/nacl/nacl_browsertest_uma.cc',
                'test/nacl/nacl_browsertest_util.cc',
                'test/nacl/nacl_browsertest_util.h',
                'test/nacl/pnacl_header_test.cc',
                'test/nacl/pnacl_header_test.h',
              ],
              'dependencies': [
                'test/data/nacl/nacl_test_data.gyp:*',
                '../ppapi/native_client/native_client.gyp:nacl_irt',
                '../ppapi/ppapi_nacl.gyp:ppapi_nacl_tests',
                '../ppapi/tests/extensions/extensions.gyp:ppapi_tests_extensions_background_keepalive',
                '../ppapi/tests/extensions/extensions.gyp:ppapi_tests_extensions_load_unload',
                '../ppapi/tests/extensions/extensions.gyp:ppapi_tests_extensions_media_galleries',
                '../ppapi/tests/extensions/extensions.gyp:ppapi_tests_extensions_multicast_permissions',
                '../ppapi/tests/extensions/extensions.gyp:ppapi_tests_extensions_no_socket_permissions',
                '../ppapi/tests/extensions/extensions.gyp:ppapi_tests_extensions_packaged_app',
                '../ppapi/tests/extensions/extensions.gyp:ppapi_tests_extensions_popup',
                '../ppapi/tests/extensions/extensions.gyp:ppapi_tests_extensions_socket_permissions',
              ],
              'conditions': [
                ['chromeos==1', {
                  'sources': [
                    '../third_party/liblouis/nacl_wrapper/liblouis_wrapper_browsertest.cc',
                  ],
                  'dependencies': [
                    'browser_chromeos',
                    '../third_party/liblouis/liblouis_nacl.gyp:liblouis_test_data',
                  ],
                }],
              ],
            }],
            ['OS=="win" or OS=="linux"', {
              'sources': [
                'browser/nacl_host/test/nacl_gdb_browsertest.cc',
              ],
              'dependencies': [
                'browser/nacl_host/test/mock_nacl_gdb.gyp:mock_nacl_gdb',
              ],
            }],
            ['OS=="win"', {
              # TODO(halyavin) NaCl on Windows can't open debug stub socket
              # in browser process as needed by this test.
              # See http://crbug.com/157312.
              'sources!': [
                'browser/nacl_host/test/gdb_debug_stub_browsertest.cc',
              ],
              'dependencies': [
                'chrome.gyp:chrome_nacl_win64',
              ],
            }],
            ['OS=="linux"', {
              'dependencies': [
                '../native_client/src/trusted/service_runtime/linux/nacl_bootstrap.gyp:nacl_helper_bootstrap',
                '../components/nacl.gyp:nacl_helper',
                '../components/nacl_nonsfi.gyp:nacl_helper_nonsfi',
              ],
            }],
            ['chromeos==0', {
              'sources!': [
                'test/data/chromeos/oobe_webui_browsertest.js',
              ],
            }],
          ],
        }],
        ['debug_devtools==1', {
          'defines': [
            'DEBUG_DEVTOOLS=1',
          ],
        }],
        ['use_ash==1', {
          'sources': [ '<@(chrome_browser_tests_ash_sources)' ],
          'dependencies': [
            '../ash/ash.gyp:ash_test_support',
          ],
          'conditions': [
            ['enable_app_list==1', {
              'sources': [
                '<@(chrome_browser_app_list_ash_test_support_sources)'
              ],
              'dependencies': [
                '../ui/app_list/presenter/app_list_presenter.gyp:app_list_presenter_test_support',
              ],
            }],
          ],
        }],
        ['use_aura==1 or toolkit_views==1', {
          'dependencies': [
            '../ui/events/events.gyp:events_test_support',
          ],
        }],
        ['chromeos == 1', {
          'sources': [ '<@(chrome_browser_tests_chromeos_sources)' ],
          'sources!': [
            # GN version: //chrome/test/browser_tests
            '../apps/load_and_launch_browsertest.cc',
            'browser/policy/policy_startup_browsertest.cc',
            'browser/printing/cloud_print/test/cloud_print_policy_browsertest.cc',
            'browser/printing/cloud_print/test/cloud_print_proxy_process_browsertest.cc',
            # chromeos does not support profile list avatar menu
            'browser/profiles/profile_list_desktop_browsertest.cc',
            'browser/service_process/service_process_control_browsertest.cc',
            # bookmark sign in promo not used on chromeos
            'browser/ui/bookmarks/bookmark_bubble_sign_in_delegate_browsertest.cc',
            # chromeos does not use cross-platform panels
            'browser/ui/panels/panel_extension_browsertest.cc',
            # inline login UI is disabled on chromeos
            'browser/ui/webui/signin/inline_login_ui_browsertest.cc',
            # chromeos does not use the desktop user manager
            'browser/ui/webui/signin/user_manager_ui_browsertest.cc',

            # GN version: //chrome/test/browser_tests_js_webui
            # chromeos does not use the desktop user manager
            'test/data/webui/md_user_manager/user_manager_browsertest.js'
          ],
          'dependencies': [
            '../build/linux/system.gyp:dbus',
            '../chromeos/ime/input_method.gyp:gencode',
            '../components/components.gyp:drive_test_support',
            '../dbus/dbus.gyp:dbus_test_support',
            '../ui/login/login.gyp:login_resources',
          ],
        }, {  # Non-ChromeOS
          'sources!': [
            # GN version: //chrome/test/browser_tests
            'browser/extensions/api/enterprise_device_attributes/enterprise_device_attributes_apitest.cc',
            'browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_apitest_nss.cc',
            'browser/extensions/api/platform_keys/platform_keys_apitest_nss.cc',
            'browser/extensions/api/terminal/terminal_private_apitest.cc',
            'browser/invalidation/profile_invalidation_provider_factory_browsertest.cc',
            'browser/net/nss_context_chromeos_browsertest.cc',
            'browser/ui/ash/keyboard_controller_browsertest.cc',
            'browser/ui/views/select_file_dialog_extension_browsertest.cc',
            'test/data/webui/certificate_viewer_dialog_test.js',
            'test/data/webui/certificate_viewer_ui_test-inl.h',
            'test/data/webui/settings/bluetooth_page_browsertest_chromeos.js',
            'test/data/webui/settings/easy_unlock_browsertest_chromeos.js',
          ],
          'conditions': [
            ['OS=="linux" or OS=="win"', {
              'sources': [
                'browser/ui/views/ime/input_ime_apitest_nonchromeos.cc',
              ]
            }],
          ]
        }],
        ['enable_web_speech==1', {
          'sources': [ '<@(chrome_browser_tests_speech_sources)' ],
        }],
        # TODO(nparker) enable tests for safe_browsing==2.
        ['safe_browsing==1', {
          'sources': [ '<@(chrome_browser_tests_full_safe_browsing_sources)' ],
          'dependencies': [
            '../components/components.gyp:safe_browsing_metadata_proto',
          ],
        }],
        ['enable_captive_portal_detection==1', {
          'sources': [ 'browser/captive_portal/captive_portal_browsertest.cc' ],
        }],
        ['enable_webrtc==0', {
          'sources!': [
            'browser/extensions/api/webrtc_audio_private/webrtc_audio_private_browsertest.cc',
            'browser/extensions/api/webrtc_logging_private/webrtc_event_log_apitest.cc',
            'browser/extensions/api/webrtc_logging_private/webrtc_logging_private_apitest.cc',
            'browser/media/webrtc_apprtc_browsertest.cc',
            'browser/media/webrtc_audio_quality_browsertest.cc',
            'browser/media/webrtc_browsertest.cc',
            'browser/media/webrtc_disable_encryption_flag_browsertest.cc',
            'browser/media/webrtc_getmediadevices_browsertest.cc',
            'browser/media/webrtc_perf_browsertest.cc',
            'browser/media/webrtc_simulcast_browsertest.cc',
            'browser/media/webrtc_video_quality_browsertest.cc',
            'browser/media/webrtc_webcam_browsertest.cc',
         ],
        }],
        ['OS=="win"', {
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_unscaled_resources.rc',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            'chrome_version_resources',
          ],
        }, { # else: OS != "win"
          'sources!': [
            'app/chrome_command_ids.h',
            'app/chrome_dll.rc',
            'app/chrome_dll_resource.h',
            'app/chrome_version.rc.version',
          ],
        }],
        ['chromeos==0 and use_ash==1', {
          'sources!': [
            # On Windows and Linux, we currently don't support enough of the
            # ash environment to run these unit tests.
            #
            # TODO: enable these on windows and linux.
            'browser/ui/ash/accelerator_commands_browsertest.cc',
            'browser/ui/ash/accelerator_controller_browsertest.cc',
            'browser/ui/ash/launcher/chrome_launcher_controller_impl_browsertest.cc',
            'browser/ui/ash/launcher/launcher_favicon_loader_browsertest.cc',
            'browser/ui/ash/shelf_browsertest.cc',
            'browser/ui/views/frame/browser_non_client_frame_view_ash_browsertest.cc',
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:nss',
          ],
        }],
        ['OS=="mac"', {
          # TODO(mark): We really want this for all non-static library
          # targets, but when we tried to pull it up to the common.gypi
          # level, it broke other things like the ui and startup tests. *shrug*
          'xcode_settings': {
            'OTHER_LDFLAGS': [
              '-Wl,-ObjC',
            ],
          },
          # Other platforms only need
          # chrome_resources.gyp:{packed_extra_resources,packed_resources},
          # and can build this target standalone much faster.
          'dependencies': [
            'app_mode_app_support',
            'chrome',
            '../components/components.gyp:breakpad_stubs',
            '../third_party/ocmock/ocmock.gyp:ocmock',
          ],
          'sources': [ '<@(chrome_browser_tests_mac_sources)' ],
          'sources!': [
            # TODO(groby): This test depends on hunspell and we cannot run it on
            # Mac, which does not use hunspell by default.
            'browser/spellchecker/spellcheck_service_browsertest.cc',

            # TODO(rouslan): This test depends on the custom dictionary UI,
            # which is disabled on Mac.
            'browser/ui/webui/options/edit_dictionary_browsertest.js',
            # TODO(rouslan): This test depends on hunspell and we cannot run it
            # on Mac, which does use hunspell by default.
            'browser/ui/webui/options/language_options_dictionary_download_browsertest.js',
            # This test depends on GetCommandLineForRelaunch, which is not
            # available on Mac.
            'browser/printing/cloud_print/test/cloud_print_policy_browsertest.cc',
            # ProcessSingletonMac doesn't do anything.
            'browser/process_singleton_browsertest.cc',
            # single-process mode hangs on Mac sometimes because of multiple UI
            # message loops. See 306348
            'renderer/safe_browsing/phishing_classifier_browsertest.cc',
            'renderer/safe_browsing/phishing_classifier_delegate_browsertest.cc',
            # This tests the language options UI features that do not exist on
            # Mac.
            'browser/ui/webui/options/multilanguage_options_webui_browsertest.js',
          ],
          'conditions': [
            # The browser window can be views or Cocoa on Mac. Test accordingly.
            ['mac_views_browser==1', {
              'sources': [ '<@(chrome_browser_tests_views_non_mac_sources)' ],
            }, {
              'sources': [ '<@(chrome_browser_tests_cocoa_sources)' ],
            }],
          ],
        }],  # OS=="mac"
        ['OS=="mac" or OS=="win"', {
          'sources': [
            'browser/extensions/api/networking_private/networking_private_apitest.cc',
            'browser/extensions/api/networking_private/networking_private_service_client_apitest.cc',
            'browser/media_galleries/fileapi/itunes_data_provider_browsertest.cc',
            'browser/media_galleries/fileapi/picasa_data_provider_browsertest.cc',
          ],
          'dependencies': [
            '../components/components.gyp:wifi_test_support',
          ],
        }],
        ['OS=="linux" or OS=="win"', {
            'sources': [ '<@(chrome_browser_tests_non_mac_desktop_sources)' ],
        }],
        ['os_posix == 0 or chromeos == 1', {
          'sources!': [
            'common/time_format_browsertest.cc',
          ],
        }],
        ['OS=="android"', {
          'sources!': [
            'browser/policy/cloud/component_cloud_policy_browsertest.cc',
            'browser/prefs/pref_hash_browsertest.cc',
            'browser/ui/bookmarks/bookmark_bubble_sign_in_delegate_browsertest.cc',
          ],
        }],
        ['chromeos == 1', {
          'sources': [
            'browser/extensions/api/networking_private/networking_private_apitest.cc',
            'browser/extensions/api/networking_private/networking_private_chromeos_apitest.cc',
          ],
        }],
        ['OS=="android" or OS=="ios" or chromeos == 1', {
          'sources!': [
            'browser/profiles/profile_statistics_browsertest.cc',
          ],
        }],
        ['toolkit_views==1', {
          'sources': [ '<@(chrome_browser_tests_views_sources)' ],
          'dependencies': [
            '../ui/views/views.gyp:views',
          ],
        }],
        ['toolkit_views==1 and OS!="mac"', {
          'sources': [ '<@(chrome_browser_tests_views_non_mac_sources)' ],
        }],
        ['toolkit_views==1 and OS!="mac" and chromeos == 0', {
          'sources': [ '<@(chrome_browser_tests_views_non_cros_or_mac_sources)' ],
        }],
        ['OS=="linux" or OS=="win"', {
          'sources': [ '<@(chrome_browser_tests_non_mac_desktop_sources)' ],
        }],
        ['OS=="ios"', {
          'sources!': [
            # TODO(dbeam): log webui URLs on iOS and test them.
            'browser/ui/webui/log_web_ui_url_browsertest.cc',
          ],
        }],
        ['OS!="android" and OS!="ios"', {
          'sources': [
            'browser/copresence/chrome_whispernet_client_browsertest.cc',
          ],
          'dependencies': [
            '../components/components.gyp:copresence',
            # build time dependency.
            '../v8/samples/samples.gyp:v8_shell#host',
          ],
        }],
        ['enable_app_list==1', {
          'sources': [ '<@(chrome_browser_tests_app_list_sources)' ],
          'conditions': [
            ['OS=="mac"', {
              'sources!': [
                # This assumes the AppList is views-based, but Mac only links
                # browser parts for the Cocoa implementation.
                'browser/ui/app_list/app_list_service_views_browsertest.cc',
              ],
            }],
          ],
        }, {
          'sources!': [ 'browser/ui/webui/app_list/start_page_browsertest.js' ],
        }],
        ['enable_service_discovery==1', {
          'sources': [ '<@(chrome_browser_tests_service_discovery_sources)' ],
        }],
        ['enable_supervised_users==1', {
          'sources': [ '<@(chrome_browser_tests_supervised_user_sources)' ],
        }],
        ['enable_pepper_cdms==1', {
          'dependencies': [
            # Runtime dependencies.
            '../media/media.gyp:clearkeycdmadapter',
            '../third_party/widevine/cdm/widevine_cdm.gyp:widevinecdmadapter',
          ],
        }],
        ['enable_print_preview==0', {
          'sources!': [
            'browser/extensions/api/cloud_print_private/cloud_print_private_apitest.cc',
            'browser/printing/cloud_print/test/cloud_print_policy_browsertest.cc',
            'browser/printing/cloud_print/test/cloud_print_proxy_process_browsertest.cc',
            'browser/printing/print_preview_dialog_controller_browsertest.cc',
            'browser/printing/print_preview_pdf_generated_browsertest.cc',
            'browser/service_process/service_process_control_browsertest.cc',
            'browser/ui/webui/print_preview/print_preview_ui_browsertest.cc',
            'test/data/webui/print_preview.cc',
            'test/data/webui/print_preview.h',
            'test/data/webui/print_preview.js',
          ],
        }],
        ['enable_media_router==1', {
          'sources': [ '<@(chrome_browser_tests_media_router_sources)' ],
          'dependencies': [
            'browser/media/router/media_router.gyp:media_router_test_support',
            'test/media_router/media_router_tests.gypi:media_router_integration_test_files'
          ],
        }],
        ['enable_mdns==1', {
          'sources' : [
            'browser/extensions/api/gcd_private/gcd_private_apitest.cc',
            'browser/ui/webui/local_discovery/local_discovery_ui_browsertest.cc',
          ]
        }],
        [ 'use_brlapi==0', {
          'sources!': [
            'browser/extensions/api/braille_display_private/braille_display_private_apitest.cc'
            ]
        }],
        ['branding=="Chrome"', {
          'sources!': [
            # crbug.com/230471
            'test/data/webui/accessibility_audit_browsertest.js',
            # These tests depend on single process mode, which is disabled in
            # official builds.
            'renderer/safe_browsing/phishing_classifier_browsertest.cc',
            'renderer/safe_browsing/phishing_classifier_delegate_browsertest.cc',
          ]
        }],
        ['OS=="android" or OS=="ios"', {
          'dependencies!': [
            '../third_party/libaddressinput/libaddressinput.gyp:libaddressinput',
          ],
        }],
        ['remoting==1', {
          'sources': [ '<@(chrome_browser_tests_remoting_sources)' ],
          'dependencies': [
            '../remoting/remoting.gyp:remoting_webapp',
          ]
        }],
        ['use_x11==1', {
          'dependencies': [
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
      ],  # conditions
    },  # target browser_tests
    {
      # Executable that runs each perf browser test in a new process.
      'target_name': 'performance_browser_tests',
      'type': 'executable',
      'dependencies': [
        'browser',
        'chrome_features.gyp:chrome_common_features',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'chrome_resources.gyp:packed_extra_resources',
        'chrome_resources.gyp:packed_resources',
        'renderer',
        'test_support_common',
        'test_support_ui',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../components/components.gyp:autofill_content_test_support',
        '../content/content_shell_and_tests.gyp:content_browser_test_base',
        '../media/cast/cast.gyp:cast_test_utility',
        '../net/net.gyp:net',
        '../net/net.gyp:net_test_support',
        '../skia/skia.gyp:skia',
        '../sync/sync.gyp:sync',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../testing/perf/perf_test.gyp:*',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        '../v8/src/v8.gyp:v8',
        # Runtime dependencies
        '../third_party/mesa/mesa.gyp:osmesa',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
      ],
      'sources': [
        '<@(performance_browser_tests_sources)',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_unscaled_resources.rc',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            'chrome_version_resources',
          ],
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_debug_link_nonincremental)',
                },
              },
            },
          }
        }, { # else: OS != "win"
          'sources!': [
            'app/chrome_command_ids.h',
            'app/chrome_dll.rc',
            'app/chrome_dll_resource.h',
            'app/chrome_version.rc.version',
          ],
        }],
        ['use_x11==1', {
          'dependencies': [
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:nss',
          ],
        }],
        ['OS=="mac"', {
          # TODO(mark): We really want this for all non-static library
          # targets, but when we tried to pull it up to the common.gypi
          # level, it broke other things like the ui and startup tests. *shrug*
          'xcode_settings': {
            'OTHER_LDFLAGS': [
              '-Wl,-ObjC',
            ],
          },
          # Other platforms only need
          # chrome_resources.gyp:{packed_extra_resources,packed_resources},
          # and can build this target standalone much faster.
          'dependencies': [
            'chrome',
            '../components/components.gyp:breakpad_stubs',
          ],
        }, {  # OS!="mac"
          'sources!': [
            'test/perf/mach_ports_performancetest.cc',
          ],
        }],
      ],  # conditions
    },  # target performance_browser_tests
    {
      # GN version: //chrome/test:sync_integration_test_support
      'target_name': 'test_support_sync_integration',
      'type': 'static_library',
      'dependencies': [
        'browser',
        'chrome',
        'test_support_common',
        '../base/base.gyp:base',
        '../components/components.gyp:invalidation_impl',
        '../components/components.gyp:invalidation_test_support',
        '../content/content_shell_and_tests.gyp:content_browser_test_base',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../sync/sync.gyp:sync',
        '../sync/sync.gyp:test_support_sync_fake_server',
        '../sync/sync.gyp:test_support_sync_testserver',
      ],
      'include_dirs': [
        '..',
        '<(INTERMEDIATE_DIR)',
        '<(protoc_out_dir)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
          '<(INTERMEDIATE_DIR)',
          '<(protoc_out_dir)',
        ],
      },
      'export_dependent_settings': [
        'browser',
      ],
      'sources': [
        '<@(test_support_sync_integration_sources)',
      ],
      'conditions': [
        ['OS=="mac"', {
          # Dictionary sync is disabled on Mac.
          # Note: this list is duplicated in the GN build.
          'sources!': [
            'browser/sync/test/integration/dictionary_helper.cc',
            'browser/sync/test/integration/dictionary_helper.h',
            'browser/sync/test/integration/dictionary_load_observer.cc',
            'browser/sync/test/integration/dictionary_load_observer.h',
          ],
        }],
        ['chromeos==0', {
          # Note: this list is duplicated in the GN build.
          'sources!': [
	    'browser/sync/test/integration/sync_arc_package_helper.cc',
            'browser/sync/test/integration/sync_arc_package_helper.h',
            'browser/sync/test/integration/wifi_credentials_helper.cc',
            'browser/sync/test/integration/wifi_credentials_helper.h',
            # 'browser/sync/test/integration/wifi_credentials_helper_chromeos.cc',
            # 'browser/sync/test/integration/wifi_credentials_helper_chromeos.h',
          ],
        }],
	['chromeos==1', {
          'dependencies': [
	    '../components/components.gyp:arc',
            '../components/components.gyp:arc_test_support',
          ],
        }],
        ['enable_app_list==1', {
          'dependencies' : [
            '../ui/app_list/app_list.gyp:app_list_test_support',
          ],
        }, {
          # Note: this list is duplicated in the GN build.
          'sources!': [
            'browser/sync/test/integration/sync_app_list_helper.cc',
            'browser/sync/test/integration/sync_app_list_helper.h',
          ],
        }],
      ]
    },
    {
      # GN version: //chrome/test:sync_integration_tests
      'target_name': 'sync_integration_tests',
      'type': 'executable',
      'dependencies': [
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'chrome_resources.gyp:packed_extra_resources',
        'chrome_resources.gyp:packed_resources',
        'common',
        'renderer',
        'test_support_common',
        'test_support_sync_integration',
        'test_support_ui',
        '../sync/sync.gyp:sync',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        '../third_party/WebKit/public/blink.gyp:blink',
      ],
      'include_dirs': [
        '..',
        '<(INTERMEDIATE_DIR)',
        '<(protoc_out_dir)',
      ],
      # TODO(phajdan.jr): Only temporary, to make transition easier.
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
      ],
      'sources': [
        '<@(sync_integration_tests_sources)',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:nss',
          ],
        }],
        ['OS=="mac"', {
          # The sync_integration_tests do not run on mac without this flag.
          # Search for comments about "xcode_settings" elsewhere in this file.
          'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
          # Dictionary sync is disabled on Mac.
          # Note: list duplicated in GN build.
          'sources!': [
            'browser/sync/test/integration/single_client_dictionary_sync_test.cc',
            'browser/sync/test/integration/two_client_dictionary_sync_test.cc',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_unscaled_resources.rc',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            'chrome_version_resources',
          ],
          'configurations': {
            'Debug': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_debug_link_nonincremental)',
                },
              },
            },
          },
        }, { # else: OS != "win"
          'sources!': [
            'app/chrome_dll.rc',
            'app/chrome_version.rc.version',
            'test/data/resource.rc',
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../ui/views/views.gyp:views',
          ],
        }],
        ['chromeos == 0', {
          'sources!': [
            # Note: this list is duplicated in the GN build.
	    "browser/sync/test/integration/single_client_arc_package_sync_test.cc",
            'browser/sync/test/integration/single_client_wifi_credentials_sync_test.cc',
	    "browser/sync/test/integration/two_client_arc_package_sync_test.cc",
            'browser/sync/test/integration/two_client_wifi_credentials_sync_test.cc',
          ],
        }],
        ['enable_basic_printing==1 or enable_print_preview==1', {
          'dependencies': [
            '../printing/printing.gyp:printing',
          ],
        }],
        ['enable_app_list==0', {
          'sources!': [
            'browser/sync/test/integration/single_client_app_list_sync_test.cc',
            'browser/sync/test/integration/two_client_app_list_sync_test.cc',
          ],
        }],
        ['enable_supervised_users==0', {
          'sources!': [
            'browser/sync/test/integration/single_client_supervised_user_settings_sync_test.cc',
          ],
        }],
      ],
    },
    {
      # GN version: //chrome/test:sync_performance_tests
      'target_name': 'sync_performance_tests',
      'type': 'executable',
      'dependencies': [
        'test_support_sync_integration',
        'test_support_ui',
        '../sync/sync.gyp:sync',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
        '<(INTERMEDIATE_DIR)',
        '<(protoc_out_dir)',
      ],
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
      ],
      'sources': [
        '<@(sync_performance_tests_sources)',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:nss',
          ],
        }],
        ['OS=="mac"', {
          # The sync_performance_tests do not run on mac without this flag.
          # Search for comments about "xcode_settings" elsewhere in this file.
          'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
          # Dictionary sync is disabled on Mac.
          'sources!': [
            'browser/sync/test/integration/performance/dictionary_sync_perf_test.cc',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            'chrome_version_resources',
          ],
          'configurations': {
            'Debug': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_debug_link_nonincremental)',
                },
              },
            },
          },
        }, { # else: OS != "win"
          'sources!': [
            'app/chrome_dll.rc',
            'app/chrome_version.rc.version',
            'test/data/resource.rc',
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../ui/views/views.gyp:views',
          ],
        }],
      ],
    },
    {
      # Executable to measure time to load libraries.
      # GN version: //chrome/test:load_library_perf_tests
      'target_name': 'load_library_perf_tests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../base/base.gyp:test_support_perf',
        '../testing/gtest.gyp:gtest',
        '../testing/perf/perf_test.gyp:*',
        '../third_party/widevine/cdm/widevine_cdm.gyp:widevine_cdm_version_h',
      ],
      'sources': [
        'browser/load_library_perf_test.cc',
      ],
      'include_dirs': [
        '..',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'conditions': [
        ['enable_pepper_cdms==1', {
          'dependencies': [
            'test_support_common',
            '../media/media.gyp:cdm_paths',
            # Runtime dependencies.
            '../media/media.gyp:clearkeycdmadapter',
            '../third_party/widevine/cdm/widevine_cdm.gyp:widevinecdmadapter',
          ],
        }],
      ],
    },  # target 'load_library_perf_tests'
  ],
  'conditions': [
    ['OS == "android"', {
        'variables' : {
           'test_support_apk_target' : 'chrome_public_test_support_apk',
           'test_support_apk_name' : 'ChromePublicTestSupport',
           'test_support_apk_manifest_path' : '../chrome/test/android/chrome_public_test_support/AndroidManifest.xml',
            'test_support_apk_dependencies' : ['cast_emulator',],

        },
        'includes' : [
            'chrome_test_support.gypi',
        ],
      'targets': [
        {
          # GN: //chrome/android:chrome_junit_tests
          'target_name': 'chrome_junit_tests',
          'type': 'none',
          'dependencies': [
            # Allow unit-testing Chrome UI components
            'android/chrome_apk.gyp:chrome_public_apk_java',
            'chrome_java',
            '../base/base.gyp:base',
            '../base/base.gyp:base_java_test_support',
            '../base/base.gyp:base_junit_test_support',
            '../sync/sync.gyp:sync_java_test_support',
            '../testing/android/junit/junit_test.gyp:junit_test_support',
          ],
          'variables': {
            'main_class': 'org.chromium.testing.local.JunitTestMain',
            'src_paths': [
              'android/junit/',
            ],
            'test_type': 'junit',
            'wrapper_script_name': 'helper/<(_target_name)',
          },
          'includes': [
            '../build/android/test_runner.gypi',
            '../build/host_jar.gypi',
          ],
        },
        {
          # GN: //chrome/test/chromedriver/test/webview_shell:chromedriver_webview_shell_apk
          'target_name': 'chromedriver_webview_shell_apk',
          'type': 'none',
          'variables': {
            'apk_name': 'ChromeDriverWebViewShell',
            'java_in_dir': 'test/chromedriver/test/webview_shell/java',
            'resource_dir': 'test/chromedriver/test/webview_shell/java/res',
          },
          'includes': [ '../build/java_apk.gypi' ],
        },
        {
          # GN: //chrome/test/android:chrome_java_test_support
          'target_name': 'chrome_java_test_support',
          'type': 'none',
          'variables': {
            'java_in_dir': '../chrome/test/android/javatests',
          },
          'dependencies': [
            'chrome_java',
            '../base/base.gyp:base_java',
            '../base/base.gyp:base_java_test_support',
            '../components/components.gyp:policy_java_test_support',
            '../content/content_shell_and_tests.gyp:content_java_test_support',
            '../net/net.gyp:net_java',
            '../net/net.gyp:net_java_test_support',
            '../sync/sync.gyp:sync_java',
            '../sync/sync.gyp:sync_java_test_support',
            '../third_party/android_tools/android_tools.gyp:google_play_services_javalib'
          ],
          'includes': [ '../build/java.gypi' ],
        },
        {
          # GN: //chrome/test/android/cast_emulator:cast_emulator
          'target_name': 'cast_emulator',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_java',
            '../third_party/android_tools/android_tools.gyp:android_support_v7_appcompat_javalib',
            '../third_party/android_tools/android_tools.gyp:android_support_v7_mediarouter_javalib',
            '../third_party/android_tools/android_tools.gyp:google_play_services_javalib',
          ],
          'variables': {
            'java_in_dir': '../chrome/test/android/cast_emulator',
          },
          'includes': [ '../build/java.gypi' ],
        },
      ],
      'conditions': [
        ['test_isolation_mode != "noop"',
          {
            'targets': [{
               'target_name': 'telemetry_perf_unittests_android_run',
               'type': 'none',
               'dependencies': [
                  '../content/content_shell_and_tests.gyp:telemetry_base',
                  'android/chrome_apk.gyp:chrome_public_apk',
               ],
               'includes': [
                 '../build/isolate.gypi',
                ],
                'sources': [
                  'telemetry_perf_unittests_android.isolate',
                ],
            }],
          }
        ],
      ],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'telemetry_chrome_test_base',
          'type': 'none',
          'dependencies': [
            '../content/content_shell_and_tests.gyp:telemetry_base',
          ],
          'conditions': [
            ['OS=="linux" or OS=="mac"', {
              'dependencies': [
                '../breakpad/breakpad.gyp:dump_syms#host',
              ],
            }],
            ['OS=="mac" or OS=="win"', {
              'dependencies': [
                '../third_party/crashpad/crashpad/tools/tools.gyp:crashpad_database_util',
              ],
            }],
            ['OS=="win"', {
              'dependencies': [
                'copy_cdb_to_output',
              ],
            }],
          ],
        },
        {
          'target_name': 'gpu_tests_base',
          'type': 'none',
          'dependencies': [
            # depend on icu to fix races. http://crbug.com/417583
            '../third_party/icu/icu.gyp:icudata',
          ],
          # Set this so we aren't included as a target in files that
          # include this file via a wildcard (such as chrome_tests.gypi).
          # If we didn't do this the All target ends up with a rule that
          # makes it unnecessarily compile in certain situations.
          'suppress_wildcard': 1,
          'direct_dependent_settings': {
            'includes': [
              '../build/isolate.gypi',
            ],
          },
        },
        {
          # GN: //gpu:angle_unittests_run
          'target_name': 'angle_unittests_run',
          'type': 'none',
          'dependencies': [
            '../gpu/gpu.gyp:angle_unittests',
            'gpu_tests_base',
          ],
          'sources': [
            'angle_unittests.isolate',
          ],
        },
        {
          'target_name': 'browser_tests_run',
          'type': 'none',
          'dependencies': [
            'browser_tests',
            'chrome',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'browser_tests.isolate',
          ],
          'conditions': [
            ['use_x11==1', {
              'dependencies': [
                '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
              ],
            }],
          ],
        },
        {
         'target_name': 'telemetry_perf_unittests_run',
         'type': 'none',
         'dependencies': [
            'chrome_run',
            'telemetry_chrome_test_base'
         ],
         'includes': [
           '../build/isolate.gypi',
          ],
          'sources': [
            'telemetry_perf_unittests.isolate',
          ],
        },
        {
         'target_name': 'telemetry_unittests_run',
         'type': 'none',
         'dependencies': [
            'chrome_run',
            'telemetry_chrome_test_base'
         ],
         'includes': [
           '../build/isolate.gypi',
          ],
          'sources': [
            'telemetry_unittests.isolate',
          ],
        },
        {
         'target_name': 'telemetry_gpu_unittests_run',
         'type': 'none',
         'dependencies': [
            '../content/content_shell_and_tests.gyp:telemetry_base',
         ],
         'includes': [
           '../build/isolate.gypi',
          ],
          'sources': [
            'telemetry_gpu_unittests.isolate',
          ],
        },
        {
          'target_name': 'chromedriver_unittests_run',
          'type': 'none',
          'dependencies': [
            'chromedriver_unittests',
          ],
          'conditions': [
            ['use_x11 == 1', {
              'dependencies': [
                '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
              ],
            }],
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'chromedriver_unittests.isolate',
          ],
        },
        {
          'target_name': 'interactive_ui_tests_run',
          'type': 'none',
          'dependencies': [
            'interactive_ui_tests',
          ],
          'conditions': [
            ['use_x11 == 1', {
              'dependencies': [
                '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
              ],
            }],
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'interactive_ui_tests.isolate',
          ],
        },
        {
          'target_name': 'sync_integration_tests_run',
          'type': 'none',
          'dependencies': [
            'sync_integration_tests',
          ],
          'conditions': [
            ['use_x11 == 1', {
              'dependencies': [
                '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
              ],
            }],
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'sync_integration_tests.isolate',
          ],
        },
      ],
      'conditions': [
        ['OS=="win"', {
          'targets': [
            {
              'target_name': 'copy_cdb_to_output',
              'type': 'none',
              'actions': [
                {
                  'action_name': 'copy_cdb',
                  'inputs': [
                    '<(DEPTH)/build/win/copy_cdb_to_output.py',
                  ],
                  'outputs': [
                    '<(PRODUCT_DIR)/cdb/cdb.exe',
                    '<(PRODUCT_DIR)/cdb/dbgeng.dll',
                    '<(PRODUCT_DIR)/cdb/dbghelp.dll',
                    '<(PRODUCT_DIR)/cdb/dbgmodel.dll',
                    '<(PRODUCT_DIR)/cdb/winext/ext.dll',
                    '<(PRODUCT_DIR)/cdb/winext/uext.dll',
                    '<(PRODUCT_DIR)/cdb/winxp/exts.dll',
                    '<(PRODUCT_DIR)/cdb/winxp/ntsdexts.dll',
                  ],
                  'action': ['python',
                             '<(DEPTH)/build/win/copy_cdb_to_output.py',
                             '<(PRODUCT_DIR)/cdb',
                             '<(target_arch)'],
                  'message': 'Copying cdb and deps to <(PRODUCT_DIR)/cdb',
                },
              ],
            },
          ],
        }],
        ['archive_gpu_tests==1', {
          'targets': [
            {
              # GN: //gpu:gl_tests_run
              'target_name': 'gl_tests_run',
              'type': 'none',
              'dependencies': [
                '../gpu/gpu.gyp:gl_tests',
                'gpu_tests_base',
              ],
              'sources': [
                'gl_tests.isolate',
              ],
            },
            {
              # GN: //chrome/test:tab_capture_end2end_tests_run
              'target_name': 'tab_capture_end2end_tests_run',
              'type': 'none',
              'dependencies': [
                'browser_tests_run',
                'gpu_tests_base',
              ],
              'sources': [
                'tab_capture_end2end_tests.isolate',
              ],
            },
            {
              'target_name': 'telemetry_gpu_integration_test_run',
              'type': 'none',
              'dependencies': [
                'chrome_run',
                'gpu_tests_base',
                'telemetry_chrome_test_base',
              ],
              'sources': [
                'telemetry_gpu_integration_test.isolate',
              ],
            },
            {
              'target_name': 'telemetry_gpu_test_run',
              'type': 'none',
              'dependencies': [
                'chrome_run',
                'gpu_tests_base',
                'telemetry_chrome_test_base',
              ],
              'sources': [
                'telemetry_gpu_test.isolate',
              ],
            },
          ],
          'conditions': [
            ['internal_gles2_conform_tests==1', {
              'targets': [
                {
                  # GN: //gpu/gles2_conform_support:gles2_conform_test_run
                  'target_name': 'gles2_conform_test_run',
                  'type': 'none',
                  'dependencies': [
                    '../gpu/gles2_conform_support/gles2_conform_test.gyp:gles2_conform_test',
                    'gpu_tests_base',
                  ],
                  'sources': [
                    'gles2_conform_test.isolate',
                  ],
                },
              ],
            }],
            ['OS=="win" or OS=="linux" or OS=="mac"', {
              'targets': [
                {
                  'target_name': 'angle_end2end_tests_run',
                  'type': 'none',
                  'dependencies': [
                    '../gpu/gpu.gyp:angle_end2end_tests',
                    'gpu_tests_base',
                  ],
                  'sources': [
                    'angle_end2end_tests.isolate',
                  ],
                },
              ],
            }],
            ['build_angle_deqp_tests==1', {
              'targets': [
                {
                  'target_name': 'angle_deqp_gles2_tests_run',
                  'type': 'none',
                  'dependencies': [
                    '../gpu/gpu.gyp:angle_deqp_gles2_tests',
                    'gpu_tests_base',
                  ],
                  'sources': [
                    'angle_deqp_gles2_tests.isolate',
                  ],
                },
                {
                  'target_name': 'angle_deqp_gles3_tests_run',
                  'type': 'none',
                  'dependencies': [
                    '../gpu/gpu.gyp:angle_deqp_gles3_tests',
                    'gpu_tests_base',
                  ],
                  'sources': [
                    'angle_deqp_gles3_tests.isolate',
                  ],
                },
                {
                  'target_name': 'angle_deqp_egl_tests_run',
                  'type': 'none',
                  'dependencies': [
                    '../gpu/gpu.gyp:angle_deqp_egl_tests',
                    'gpu_tests_base',
                  ],
                  'sources': [
                    'angle_deqp_egl_tests.isolate',
                  ],
                },
              ],
            }],
          ],
        }],
      ],
    }],
    [ 'enable_mdns == 1', {
      'targets': [{
          # GN version: //chrome/tools/service_discovery_sniffer
          'target_name': 'service_discovery_sniffer',
          'type': 'executable',
          'dependencies': [
            '../net/net.gyp:net',
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
            # TODO(vitalybuka): Extract mdns code into lib or component.
            'browser',
          ],
          'sources': [
            'tools/service_discovery_sniffer/service_discovery_sniffer.cc',
            'tools/service_discovery_sniffer/service_discovery_sniffer.h',
          ],
        }]
    }],
  ],  # 'conditions'
}
