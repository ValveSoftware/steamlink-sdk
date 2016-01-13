# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'remoting_webapp_info_files': [
      'resources/chromoting16.webp',
      'resources/chromoting48.webp',
      'resources/chromoting128.webp',
    ],

    # Jscompile proto files.
    # These provide type information for jscompile.
    'remoting_webapp_js_proto_files': [
      'webapp/js_proto/chrome_proto.js',
      'webapp/js_proto/console_proto.js',
      'webapp/js_proto/dom_proto.js',
      'webapp/js_proto/remoting_proto.js',
    ],

    # Auth (apps v1) JavaScript files.
    # These files aren't included directly from main.html. They are
    # referenced from the manifest.json file (appsv1 only).
    'remoting_webapp_js_auth_v1_files': [
      'webapp/cs_third_party_auth_trampoline.js',  # client to host
      'webapp/cs_oauth2_trampoline.js',  # Google account
    ],
    # Auth (client to host) JavaScript files.
    'remoting_webapp_js_auth_client2host_files': [
      'webapp/third_party_host_permissions.js',
      'webapp/third_party_token_fetcher.js',
    ],
    # Auth (Google account) JavaScript files.
    'remoting_webapp_js_auth_google_files': [
      'webapp/identity.js',
      'webapp/oauth2.js',
      'webapp/oauth2_api.js',
    ],
    # Client JavaScript files.
    'remoting_webapp_js_client_files': [
      'webapp/client_plugin.js',
      # TODO(garykac) For client_screen:
      # * Split out pin/access code stuff into separate file.
      # * Move client logic into session_connector
      'webapp/client_screen.js',
      'webapp/client_session.js',
      'webapp/clipboard.js',
      'webapp/media_source_renderer.js',
      'webapp/session_connector.js',
      'webapp/smart_reconnector.js',
    ],
    # Remoting core JavaScript files.
    'remoting_webapp_js_core_files': [
      'webapp/base.js',
      'webapp/error.js',
      'webapp/event_handlers.js',
      'webapp/plugin_settings.js',
      # TODO(garykac) Split out UI client stuff from remoting.js.
      'webapp/remoting.js',
      'webapp/typecheck.js',
      'webapp/xhr.js',
    ],
    # Host JavaScript files.
    # Includes both it2me and me2me files.
    'remoting_webapp_js_host_files': [
      'webapp/host_controller.js',
      'webapp/host_dispatcher.js',
      'webapp/host_it2me_dispatcher.js',
      'webapp/host_it2me_native_messaging.js',
      'webapp/host_native_messaging.js',
      'webapp/host_session.js',
    ],
    # Logging and stats JavaScript files.
    'remoting_webapp_js_logging_files': [
      'webapp/format_iq.js',
      'webapp/log_to_server.js',
      'webapp/server_log_entry.js',
      'webapp/stats_accumulator.js',
    ],
    # UI JavaScript files.
    'remoting_webapp_js_ui_files': [
      'webapp/butter_bar.js',
      'webapp/connection_stats.js',
      'webapp/feedback.js',
      'webapp/fullscreen.js',
      'webapp/fullscreen_v1.js',
      'webapp/fullscreen_v2.js',
      'webapp/l10n.js',
      'webapp/menu_button.js',
      'webapp/ui_mode.js',
      'webapp/toolbar.js',
      'webapp/window_frame.js',
    ],
    # UI files for controlling the local machine as a host.
    'remoting_webapp_js_ui_host_control_files': [
      'webapp/host_screen.js',
      'webapp/host_setup_dialog.js',
      'webapp/host_install_dialog.js',
      'webapp/paired_client_manager.js',
    ],
    # UI files for displaying (in the client) info about available hosts.
    'remoting_webapp_js_ui_host_display_files': [
      'webapp/host.js',
      'webapp/host_list.js',
      'webapp/host_settings.js',
      'webapp/host_table_entry.js',
    ],
    # Remoting WCS container JavaScript files.
    'remoting_webapp_js_wcs_container_files': [
      'webapp/wcs_sandbox_container.js',
    ],
    # Remoting WCS sandbox JavaScript files.
    'remoting_webapp_js_wcs_sandbox_files': [
      'webapp/wcs.js',
      'webapp/wcs_loader.js',
      'webapp/wcs_sandbox_content.js',
      'webapp/xhr_proxy.js',
    ],
    # gnubby authentication JavaScript files.
    'remoting_webapp_js_gnubby_auth_files': [
      'webapp/gnubby_auth_handler.js',
    ],
    # browser test JavaScript files.
    'remoting_webapp_js_browser_test_files': [
      'webapp/browser_test/browser_test.js',
      'webapp/browser_test/cancel_pin_browser_test.js',
      'webapp/browser_test/invalid_pin_browser_test.js',
      'webapp/browser_test/update_pin_browser_test.js',
    ],
    # The JavaScript files required by main.html.
    'remoting_webapp_main_html_js_files': [
      # Include the core files first as it is required by the other files.
      # Otherwise, Jscompile will complain.
      '<@(remoting_webapp_js_core_files)',
      '<@(remoting_webapp_js_auth_client2host_files)',
      '<@(remoting_webapp_js_auth_google_files)',
      '<@(remoting_webapp_js_client_files)',
      '<@(remoting_webapp_js_gnubby_auth_files)',
      '<@(remoting_webapp_js_host_files)',
      '<@(remoting_webapp_js_logging_files)',
      '<@(remoting_webapp_js_ui_files)',
      '<@(remoting_webapp_js_ui_host_control_files)',
      '<@(remoting_webapp_js_ui_host_display_files)',
      '<@(remoting_webapp_js_wcs_container_files)',
      # Uncomment this line to include browser test files in the web app
      # to expedite debugging or local development.
      # '<@(remoting_webapp_js_browser_test_files)'
    ],

    # The JavaScript files required by wcs_sandbox.html.
    'remoting_webapp_wcs_sandbox_html_js_files': [
      '<@(remoting_webapp_js_wcs_sandbox_files)',
      'webapp/error.js',
      'webapp/plugin_settings.js',
    ],

    # All the JavaScript files required by the webapp.
    'remoting_webapp_all_js_files': [
      # JS files for main.html.
      '<@(remoting_webapp_main_html_js_files)',
      # JS files for wcs_sandbox.html.
      # Use r_w_js_wcs_sandbox_files instead of r_w_wcs_sandbox_html_js_files
      # so that we don't double include error.js and plugin_settings.js.
      '<@(remoting_webapp_js_wcs_sandbox_files)',
      # JS files referenced in mainfest.json.
      '<@(remoting_webapp_js_auth_v1_files)',
    ],

    'remoting_webapp_resource_files': [
      'resources/disclosure_arrow_down.webp',
      'resources/disclosure_arrow_right.webp',
      'resources/drag.webp',
      'resources/host_setup_instructions.webp',
      'resources/icon_close.webp',
      'resources/icon_cross.webp',
      'resources/icon_disconnect.webp',
      'resources/icon_help.webp',
      'resources/icon_host.webp',
      'resources/icon_maximize_restore.webp',
      'resources/icon_minimize.webp',
      'resources/icon_pencil.webp',
      'resources/icon_warning.webp',
      'resources/infographic_my_computers.webp',
      'resources/infographic_remote_assistance.webp',
      'resources/plus.webp',
      'resources/reload.webp',
      'resources/tick.webp',
      'webapp/connection_stats.css',
      'webapp/main.css',
      'webapp/menu_button.css',
      'webapp/open_sans.css',
      'webapp/open_sans.woff',
      'webapp/scale-to-fit.webp',
      'webapp/spinner.gif',
      'webapp/toolbar.css',
      'webapp/window_frame.css',
    ],

    'remoting_webapp_files': [
      '<@(remoting_webapp_info_files)',
      '<@(remoting_webapp_all_js_files)',
      '<@(remoting_webapp_resource_files)',
    ],

    # These template files are used to construct the webapp html files.
    'remoting_webapp_template_main':
      'webapp/html/template_main.html',

    'remoting_webapp_template_wcs_sandbox':
      'webapp/html/template_wcs_sandbox.html',

    'remoting_webapp_template_files': [
      'webapp/html/butterbar.html',
      'webapp/html/client_plugin.html',
      'webapp/html/dialog_auth.html',
      'webapp/html/dialog_client_connect_failed.html',
      'webapp/html/dialog_client_connecting.html',
      'webapp/html/dialog_client_host_needs_upgrade.html',
      'webapp/html/dialog_client_pin_prompt.html',
      'webapp/html/dialog_client_session_finished.html',
      'webapp/html/dialog_client_third_party_auth.html',
      'webapp/html/dialog_client_unconnected.html',
      'webapp/html/dialog_confirm_host_delete.html',
      'webapp/html/dialog_connection_history.html',
      'webapp/html/dialog_host.html',
      'webapp/html/dialog_host_install.html',
      'webapp/html/dialog_host_setup.html',
      'webapp/html/dialog_manage_pairings.html',
      'webapp/html/dialog_token_refresh_failed.html',
      'webapp/html/toolbar.html',
      'webapp/html/ui_header.html',
      'webapp/html/ui_it2me.html',
      'webapp/html/ui_me2me.html',
      'webapp/html/window_frame.html',
    ],

  },
}
