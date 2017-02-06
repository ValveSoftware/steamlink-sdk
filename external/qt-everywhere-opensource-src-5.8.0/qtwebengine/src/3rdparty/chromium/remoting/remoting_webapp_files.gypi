# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# `7MM"""Mq.                       `7MM
#   MM   `MM.                        MM
#   MM   ,M9  .gP"Ya   ,6"Yb.   ,M""bMM  `7MMpMMMb.pMMMb.  .gP"Ya
#   MMmmdM9  ,M'   Yb 8)   MM ,AP    MM    MM    MM    MM ,M'   Yb
#   MM  YM.  8M""""""  ,pm9MM 8MI    MM    MM    MM    MM 8M""""""
#   MM   `Mb.YM.    , 8M   MM `Mb    MM    MM    MM    MM YM.    ,
# .JMML. .JMM.`Mbmmd' `Moo9^Yo.`Wbmd"MML..JMML  JMML  JMML.`Mbmmd'
#
# Please keep this file in sync with remoting/webapp/files.gni.

{
  'variables': {

    # JSCompiler externs.
    'remoting_webapp_js_externs_files': [
      'webapp/externs/qunit.js',
    ],

    # Jscompile proto files.
    # These provide type information for jscompile.
    'remoting_webapp_js_proto_files': [
      'webapp/js_proto/chrome_proto.js',
      'webapp/js_proto/dom_proto.js',
      'webapp/js_proto/remoting_proto.js',
    ],

    #
    # Webapp browsertest JavaScript files.
    #

    # Browser test files.
    'remoting_webapp_browsertest_js_files': [
      'webapp/browser_test/browser_test.js',
      'webapp/browser_test/bump_scroll_browser_test.js',
      'webapp/browser_test/cancel_pin_browser_test.js',
      'webapp/browser_test/invalid_pin_browser_test.js',
      'webapp/browser_test/it2me_browser_test.js',
      'webapp/browser_test/me2me_browser_test.js',
      'webapp/browser_test/scrollbar_browser_test.js',
      'webapp/browser_test/timeout_waiter.js',
      'webapp/browser_test/unauthenticated_browser_test.js',
      'webapp/browser_test/update_pin_browser_test.js',
    ],
    # Browser test files.
    'remoting_webapp_browsertest_js_mock_files': [
      'webapp/crd/js/mock_client_plugin.js',
      'webapp/crd/js/mock_host_list_api.js',
      'webapp/crd/js/mock_identity.js',
      'webapp/crd/js/mock_oauth2_api.js',
      'webapp/crd/js/mock_signal_strategy.js',
    ],
    'remoting_webapp_browsertest_js_proto_files': [
      'webapp/js_proto/sinon_proto.js',
      'webapp/js_proto/test_proto.js',
      '<@(remoting_webapp_js_proto_files)',
    ],
    'remoting_webapp_browsertest_all_js_files': [
      '<@(remoting_webapp_browsertest_js_files)',
      '<@(remoting_webapp_browsertest_js_mock_files)',
    ],
    'remoting_webapp_browsertest_main_html_extra_files': [
      '<@(remoting_webapp_browsertest_all_js_files)',
      '../third_party/sinonjs/src/sinon.js',
    ],

    #
    # Webapp unittest JavaScript files.
    #

    # These product files are excluded from our JavaScript unittest
    'remoting_webapp_unittests_exclude_js_files': [
      # background.js is where the onLoad handler is defined, which
      # makes it the entry point of the background page.
      'webapp/crd/js/background.js',
    ],
    # The unit test cases for the webapp
    'remoting_webapp_unittests_js_files': [
      'webapp/base/js/base_unittest.js',
      'webapp/base/js/base_event_hook_unittest.js',
      'webapp/base/js/base_inherits_unittest.js',
      'webapp/base/js/callstack_unittest.js',
      'webapp/base/js/client_session_unittest.js',
      'webapp/base/js/client_session_factory_unittest.js',
      'webapp/base/js/console_wrapper_unittest.js',
      'webapp/base/js/dns_blackhole_checker_unittest.js',
      'webapp/base/js/error_unittest.js',
      'webapp/base/js/fallback_signal_strategy_unittest.js',
      'webapp/base/js/host_options_unittest.js',
      'webapp/base/js/identity_unittest.js',
      'webapp/base/js/ipc_unittest.js',
      'webapp/base/js/l10n_unittest.js',
      'webapp/base/js/network_connectivity_detector_unittest.js',
      'webapp/base/js/platform_unittest.js',
      'webapp/base/js/protocol_extension_manager_unittest.js',
      'webapp/base/js/session_logger_unittest.js',
      'webapp/base/js/telemetry_event_writer_unittest.js',
      'webapp/base/js/typecheck_unittest.js',
      'webapp/base/js/viewport_unittest.js',
      'webapp/base/js/window_shape_unittest.js',
      'webapp/base/js/window_message_dispatcher_unittest.js',
      'webapp/base/js/xhr_event_writer_unittest.js',
      'webapp/base/js/xhr_unittest.js',
      'webapp/base/js/xmpp_connection_unittest.js',
      'webapp/base/js/xmpp_error_cache_unittest.js',
      'webapp/base/js/xmpp_login_handler_unittest.js',
      'webapp/base/js/xmpp_stream_parser_unittest.js',
      'webapp/crd/js/apps_v2_migration_unittest.js',
      'webapp/crd/js/combined_host_list_api_unittest.js',
      'webapp/crd/js/gcd_client_unittest.js',
      'webapp/crd/js/gcd_client_with_mock_xhr_unittest.js',
      "webapp/crd/js/gnubby_auth_handler_unittest.js",
      'webapp/crd/js/host_controller_unittest.js',
      'webapp/crd/js/host_daemon_facade_unittest.js',
      'webapp/crd/js/host_table_entry_unittest.js',
      'webapp/crd/js/legacy_host_list_api_unittest.js',
      'webapp/crd/js/menu_button_unittest.js',
      'webapp/crd/js/me2me_telemetry_integration_test.js',
      'webapp/crd/js/mock_xhr_unittest.js',
    ],
    'remoting_webapp_unittests_js_mock_files': [
      'webapp/crd/js/mock_client_plugin.js',
      'webapp/crd/js/mock_host_daemon_facade.js',
      'webapp/crd/js/mock_host_list_api.js',
      'webapp/crd/js/mock_modal_dialog_factory.js',
      'webapp/crd/js/mock_identity.js',
      'webapp/crd/js/mock_signal_strategy.js',
      'webapp/crd/js/remoting_activity_test_driver.js',
      'webapp/js_proto/chrome_mocks.js',
      'webapp/unittests/sinon_helpers.js',
      'webapp/crd/js/mock_xhr.js',
    ],
    # Prototypes for objects that are not mocked.
    'remoting_webapp_unittests_js_proto_files': [
      'webapp/js_proto/chrome_proto.js',
      'webapp/js_proto/dom_proto.js',
      'webapp/js_proto/remoting_proto.js',
      'webapp/js_proto/sinon_proto.js',
    ],
    'remoting_webapp_unittests_all_js_files': [
      '<@(remoting_webapp_unittests_js_files)',
      '<@(remoting_webapp_unittests_js_mock_files)',
      'webapp/unittests/qunit_callbacks.js',
    ],
    # All the files needed to run the unittests.
    'remoting_webapp_unittests_all_files': [
      'webapp/crd/html/menu_button.css',
      '<@(remoting_webapp_unittests_all_js_files)',
    ],
    'remoting_webapp_unittests_template_main':
      'webapp/crd/html/template_unittests.html',

    #
    # Shared webapp JavaScript file groups.
    #

    # Auth (Google account) JavaScript files.
    'remoting_webapp_shared_js_auth_google_files': [
      'webapp/base/js/auth_init.js',
      'webapp/base/js/identity.js',
      'webapp/base/js/oauth2.js',
      'webapp/base/js/oauth2_api.js',
      'webapp/base/js/oauth2_api_impl.js',
    ],
    # Client JavaScript files.
    'remoting_webapp_shared_js_client_files': [
      'webapp/base/js/activity.js',
      'webapp/base/js/client_plugin.js',
      'webapp/base/js/client_plugin_host_desktop_impl.js',
      'webapp/base/js/client_plugin_impl.js',
      'webapp/base/js/client_session.js',
      'webapp/base/js/client_session_factory.js',
      'webapp/base/js/clipboard.js',
      'webapp/base/js/connected_view.js',
      'webapp/base/js/connection_info.js',
      'webapp/base/js/credentials_provider.js',
      'webapp/base/js/experiments.js',
      'webapp/base/js/host_desktop.js',
      'webapp/base/js/telemetry_event_writer.js',
      'webapp/base/js/xmpp_error_cache.js',
    ],
    # Remoting core JavaScript files.
    'remoting_webapp_shared_js_core_files': [
      'webapp/base/js/application.js',
      'webapp/base/js/base.js',
      'webapp/base/js/callstack.js',
      'webapp/base/js/console_wrapper.js',
      'webapp/base/js/ipc.js',
      'webapp/base/js/modal_dialogs.js',
      'webapp/base/js/platform.js',
      'webapp/base/js/protocol_extension_manager.js',
      'webapp/base/js/protocol_extension.js',
      'webapp/base/js/error.js',
      'webapp/base/js/network_connectivity_detector.js',
      'webapp/base/js/plugin_settings.js',
      'webapp/base/js/suspend_detector.js',
      'webapp/base/js/typecheck.js',
      'webapp/base/js/xhr_event_writer.js',
      'webapp/base/js/window_message_dispatcher.js',
      'webapp/base/js/xhr.js',
    ],
    # Host JavaScript files.
    'remoting_webapp_shared_js_host_files': [
      'webapp/base/js/host.js',
      'webapp/base/js/host_options.js',
    ],
    # Logging and stats JavaScript files.
    'remoting_webapp_shared_js_logging_files': [
      'webapp/base/js/chromoting_event.js',
      'webapp/base/js/format_iq.js',
      'webapp/base/js/session_logger.js',
      'webapp/base/js/stats_accumulator.js',
    ],
    # Remoting signaling files.
    'remoting_webapp_shared_js_signaling_files': [
      'webapp/base/js/dns_blackhole_checker.js',
      'webapp/base/js/fallback_signal_strategy.js',
      'webapp/base/js/signal_strategy.js',
      'webapp/base/js/tcp_socket.js',
      'webapp/base/js/wcs_adapter.js',
      'webapp/base/js/wcs_sandbox_container.js',
      'webapp/base/js/xmpp_connection.js',
      'webapp/base/js/xmpp_login_handler.js',
      'webapp/base/js/xmpp_stream_parser.js',
    ],
    # Shared UI JavaScript files.
    'remoting_webapp_shared_js_ui_files': [
      'webapp/base/js/connection_dropped_dialog.js',
      'webapp/base/js/connection_stats.js',
      'webapp/base/js/l10n.js',
      'webapp/base/js/ui_mode.js',
      'webapp/base/js/viewport.js',
      'webapp/base/js/window_shape.js',
    ],

    #
    # DesktopRemoting webapp JavaScript file groups.
    #

    # Auth (apps v1) JavaScript files.
    # These files aren't included directly from main.html. They are
    # referenced from the manifest.json file (appsv1 only).
    'remoting_webapp_js_auth_v1_files': [
      'webapp/crd/js/cs_third_party_auth_trampoline.js',  # client to host
      'webapp/crd/js/cs_oauth2_trampoline.js',  # Google account
    ],
    # Auth (client to host) JavaScript files.
    'remoting_webapp_js_auth_client2host_files': [
      'webapp/crd/js/third_party_host_permissions.js',
      'webapp/crd/js/third_party_token_fetcher.js',
    ],
    # Client JavaScript files.
    'remoting_webapp_js_client_files': [
      'webapp/crd/js/apps_v2_migration.js',
      'webapp/crd/js/event_handlers.js',
      'webapp/crd/js/gcd_client.js',
    ],
    # Gnubby authentication JavaScript files.
    'remoting_webapp_js_gnubby_auth_files': [
      'webapp/crd/js/gnubby_auth_handler.js',
    ],
    # Files for controlling the local machine as a host.
    # Includes both it2me and me2me files.
    'remoting_webapp_js_host_control_files': [
      'webapp/crd/js/host_controller.js',
      'webapp/crd/js/host_daemon_facade.js',
      'webapp/crd/js/host_screen.js',
      'webapp/crd/js/host_session.js',
      'webapp/crd/js/host_setup_dialog.js',
      'webapp/crd/js/host_install_dialog.js',
      'webapp/crd/js/host_installer.js',
      'webapp/crd/js/it2me_host_facade.js',
      'webapp/crd/js/native_message_host_log_message_handler.js',
      'webapp/crd/js/paired_client_manager.js',
    ],
    # Files for controlling the local it2me host.
    # Included by public_session.html.
    'remoting_webapp_js_it2me_host_control_files': [
      'webapp/crd/js/host_screen.js',
      'webapp/crd/js/host_session.js',
      'webapp/crd/js/host_install_dialog.js',
      'webapp/crd/js/host_installer.js',
      'webapp/crd/js/it2me_host_facade.js',
      'webapp/crd/js/native_message_host_log_message_handler.js',
    ],
    # Files for displaying (in the client) info about available hosts.
    'remoting_webapp_js_host_display_files': [
      'webapp/crd/js/host_list.js',
      'webapp/crd/js/host_list_api.js',
      'webapp/crd/js/host_table_entry.js',
      'webapp/crd/js/local_host_section.js',

      # Must come after host_list_api.js because of an issue with
      # JSCompiler.  If an implementation of an interface occurs in a
      # file processed before the interface itself, the @override tag
      # doesn't always work correctly.
      'webapp/crd/js/combined_host_list_api.js',
      'webapp/crd/js/gcd_host_list_api.js',
      'webapp/crd/js/legacy_host_list_api.js',
    ],
    # The CRD-specific JavaScript files required by main.html.
    'remoting_webapp_crd_js_ui_files': [
      'webapp/crd/js/bump_scroller.js',
      'webapp/crd/js/butter_bar.js',
      'webapp/crd/js/crd_auth_dialog.js',
      'webapp/crd/js/crd_event_handlers.js',
      'webapp/crd/js/crd_experimental.js',
      'webapp/crd/js/desktop_connected_view.js',
      'webapp/crd/js/desktop_remoting.js',
      'webapp/crd/js/desktop_remoting_activity.js',
      'webapp/crd/js/desktop_viewport.js',
      'webapp/crd/js/feedback.js',
      'webapp/crd/js/fullscreen.js',
      'webapp/crd/js/fullscreen_v1.js',
      'webapp/crd/js/fullscreen_v2.js',
      'webapp/crd/js/it2me_activity.js',
      'webapp/crd/js/me2me_activity.js',
      'webapp/crd/js/menu_button.js',
      'webapp/crd/js/options_menu.js',
      'webapp/crd/js/toolbar.js',
      'webapp/crd/js/window_frame.js',
    ],

    #
    # DesktopRemoting main.html generation files.
    #

    'remoting_webapp_template_main':
      '<(DEPTH)/remoting/webapp/crd/html/template_main.html',

    # The shared JavaScript files required by main.html.
    # These files are shared with the unittest target.
    'remoting_webapp_shared_main_html_js_files': [
      # Include the core files first as it is required by the other files.
      # Otherwise, Jscompile will complain.
      '<@(remoting_webapp_shared_js_core_files)',
      '<@(remoting_webapp_shared_js_auth_google_files)',
      '<@(remoting_webapp_shared_js_client_files)',
      '<@(remoting_webapp_shared_js_host_files)',
      '<@(remoting_webapp_shared_js_logging_files)',
      '<@(remoting_webapp_shared_js_signaling_files)',
      '<@(remoting_webapp_shared_js_ui_files)',
      '<@(remoting_webapp_js_auth_client2host_files)',
      '<@(remoting_webapp_js_client_files)',
      '<@(remoting_webapp_js_gnubby_auth_files)',
      '<@(remoting_webapp_js_host_control_files)',
      '<@(remoting_webapp_js_host_display_files)',
    ],

    'remoting_webapp_template_public_session':
      '<(DEPTH)/remoting/webapp/crd/html/template_public_session.html',

    # The shared JavaScript files required by public_session.html.
    'remoting_webapp_public_session_html_all_js_files': [
      '<@(remoting_webapp_shared_js_core_files)',
      '<@(remoting_webapp_shared_js_auth_google_files)',
      '<@(remoting_webapp_shared_js_client_files)',
      '<@(remoting_webapp_shared_js_host_files)',
      '<@(remoting_webapp_shared_js_logging_files)',
      '<@(remoting_webapp_shared_js_signaling_files)',
      '<@(remoting_webapp_shared_js_ui_files)',
      '<@(remoting_webapp_js_it2me_host_control_files)',
      'webapp/crd/js/public_session_main.js',
    ],

    # These template files are used to construct public_session.html.
    'remoting_webapp_public_session_template_files': [
      'webapp/crd/html/dialog_host.html',
    ],

    # The JS files required by unittest.html.
    'remoting_webapp_unittest_html_all_js_files': [
      '<@(remoting_webapp_shared_main_html_js_files)',
      '<@(remoting_webapp_crd_js_ui_files)',
    ],

    # The CRD-specific JavaScript files required by main.html.
    'remoting_webapp_crd_main_html_all_js_files': [
      '<@(remoting_webapp_shared_main_html_js_files)',
      '<@(remoting_webapp_crd_js_ui_files)',
      'webapp/crd/js/crd_main.js',
    ],

    # These template files are used to construct main.html.
    'remoting_webapp_template_files': [
      'webapp/base/html/client_plugin.html',
      'webapp/crd/html/butter_bar.html',
      'webapp/crd/html/dialog_auth.html',
      'webapp/crd/html/dialog_client_connect_failed.html',
      'webapp/crd/html/dialog_client_connecting.html',
      'webapp/crd/html/dialog_client_host_needs_upgrade.html',
      'webapp/crd/html/dialog_client_pin_prompt.html',
      'webapp/crd/html/dialog_client_session_finished.html',
      'webapp/crd/html/dialog_client_third_party_auth.html',
      'webapp/crd/html/dialog_client_unconnected.html',
      'webapp/crd/html/dialog_confirm_host_delete.html',
      'webapp/crd/html/dialog_connection_history.html',
      'webapp/crd/html/dialog_host.html',
      'webapp/crd/html/dialog_host_install.html',
      'webapp/crd/html/dialog_host_setup.html',
      'webapp/crd/html/dialog_manage_pairings.html',
      'webapp/crd/html/dialog_token_refresh_failed.html',
      'webapp/crd/html/toolbar.html',
      'webapp/crd/html/ui_header.html',
      'webapp/crd/html/ui_it2me.html',
      'webapp/crd/html/ui_me2me.html',
      'webapp/crd/html/window_frame.html',
    ],

    #
    # Webapp background.html generation files.
    #

    'remoting_webapp_template_background':
      '<(DEPTH)/remoting/webapp/crd/html/template_background.html',

    # These JS files are specific to the background page and are not part of
    # the main JS files.
    'remoting_webapp_background_html_js_files': [
      'webapp/base/js/chromoting_event.js',
      'webapp/crd/js/activation_handler.js',
      'webapp/crd/js/app_launcher.js',
      'webapp/crd/js/background.js',
    ],

    # All the JavaScript files required by background.html.
    'remoting_webapp_background_html_all_js_files': [
      '<@(remoting_webapp_background_html_js_files)',
      'webapp/base/js/base.js',
      'webapp/base/js/error.js',
      'webapp/base/js/identity.js',
      'webapp/base/js/ipc.js',
      'webapp/base/js/l10n.js',
      'webapp/base/js/oauth2.js',
      'webapp/base/js/oauth2_api.js',
      'webapp/base/js/oauth2_api_impl.js',
      'webapp/base/js/platform.js',
      'webapp/base/js/plugin_settings.js',
      'webapp/base/js/telemetry_event_writer.js',
      'webapp/base/js/typecheck.js',
      'webapp/base/js/xhr.js',
      'webapp/base/js/xhr_event_writer.js',
      'webapp/crd/js/host_installer.js',
      'webapp/crd/js/host_session.js',
      'webapp/crd/js/it2me_host_facade.js',
    ],

    #
    # Webapp wcs_sandbox.html generation files.
    #

    'remoting_webapp_template_wcs_sandbox':
      '<(DEPTH)/remoting/webapp/base/html/template_wcs_sandbox.html',

    # These JS files are specific to the WCS sandbox page and are not part of
    # the main JS files.
    'remoting_webapp_wcs_sandbox_html_js_files': [
      'webapp/crd/js/wcs.js',
      'webapp/crd/js/wcs_loader.js',
      'webapp/crd/js/wcs_sandbox_content.js',
      'webapp/crd/js/xhr_proxy.js',
    ],

    # All the JavaScript files required by wcs_sandbox.html.
    'remoting_webapp_wcs_sandbox_html_all_js_files': [
      '<@(remoting_webapp_wcs_sandbox_html_js_files)',
      'webapp/base/js/error.js',
      'webapp/base/js/plugin_settings.js',
    ],

    #
    # Webapp message_window.html generation files.
    #

    'remoting_webapp_template_message_window':
      '<(DEPTH)/remoting/webapp/base/html/template_message_window.html',

    # These JS files are specific to the message window page and are not part of
    # the main JS files.
    'remoting_webapp_message_window_html_js_files': [
      'webapp/base/js/message_window.js',
    ],

    # All the JavaScript files required by message_window.html.
    'remoting_webapp_message_window_html_all_js_files': [
      '<@(remoting_webapp_message_window_html_js_files)',
      'webapp/base/js/base.js',
    ],

    #
    # All the JavaScript files required by credits.html
    #
    'remoting_webapp_credits_html_all_js_files': [
      'webapp/base/js/credits_js.js',
    ],

    #
    # Complete webapp JS and resource files.
    #

    # All the JavaScript files that are shared by webapps.
    'remoting_webapp_shared_js_files': [
      '<@(remoting_webapp_shared_main_html_js_files)',
      '<@(remoting_webapp_background_html_js_files)',
      '<@(remoting_webapp_message_window_html_js_files)',
      '<@(remoting_webapp_wcs_sandbox_html_js_files)',
      # JS files referenced in manifest.json.
      '<@(remoting_webapp_js_auth_v1_files)',
    ],

    # All the JavaScript files required by DesktopRemoting.
    'remoting_webapp_crd_js_files': [
      '<@(remoting_webapp_shared_js_files)',
      '<@(remoting_webapp_crd_main_html_all_js_files)',
      '<@(remoting_webapp_credits_html_all_js_files)',
      '<@(remoting_webapp_public_session_html_all_js_files)',
    ],

    'remoting_webapp_info_files': [
      'resources/chromoting16.webp',
      'resources/chromoting48.webp',
      'resources/chromoting128.webp',
    ],

    # All the resource files required by DesktopRemoting.
    'remoting_webapp_resource_files': [
      'resources/disclosure_arrow_down.webp',
      'resources/disclosure_arrow_right.webp',
      'resources/drag.webp',
      'resources/host_setup_instructions.webp',
      'resources/icon_close.webp',
      'resources/icon_cross.webp',
      'resources/icon_disconnect.webp',
      'resources/icon_fullscreen.webp',
      'resources/icon_host.webp',
      'resources/icon_maximize_restore.webp',
      'resources/icon_minimize.webp',
      'resources/icon_options.webp',
      'resources/icon_pencil.webp',
      'resources/icon_warning.webp',
      'resources/infographic_my_computers.webp',
      'resources/infographic_remote_assistance.webp',
      'resources/plus.webp',
      'resources/reload.webp',
      'resources/tick.webp',
      'webapp/base/html/connection_stats.css',
      'webapp/base/html/credits_css.css',
      'webapp/base/html/main.css',
      'webapp/base/html/message_window.css',
      'webapp/base/resources/open_sans.css',
      'webapp/base/resources/open_sans.woff',
      'webapp/base/resources/spinner.gif',
      'webapp/crd/html/butter_bar.css',
      'webapp/crd/html/crd_main.css',
      'webapp/crd/html/toolbar.css',
      'webapp/crd/html/menu_button.css',
      'webapp/crd/html/public_session.css',
      'webapp/crd/html/window_frame.css',
      'webapp/crd/resources/scale-to-fit.webp',
    ],

    'remoting_webapp_crd_files': [
      '<@(remoting_webapp_info_files)',
      '<@(remoting_webapp_crd_js_files)',
      '<@(remoting_webapp_resource_files)',
    ],

    # Files that contain localizable strings.
    'desktop_remoting_webapp_localizable_files': [
      'webapp/crd/manifest.json.jinja2',
      '<(remoting_webapp_template_background)',
      '<(remoting_webapp_template_main)',
      '<(remoting_webapp_template_message_window)',
      '<(remoting_webapp_template_wcs_sandbox)',
      '<@(remoting_webapp_template_files)',
      '<@(remoting_webapp_crd_js_files)',
    ],

  },
}
