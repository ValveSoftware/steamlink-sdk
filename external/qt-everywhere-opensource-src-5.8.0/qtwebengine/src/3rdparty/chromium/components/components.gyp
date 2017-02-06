# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # This turns on e.g. the filename-based detection of which
    # platforms to include source files on (e.g. files ending in
    # _mac.h or _mac.cc are only compiled on MacOSX).
    'chromium_code': 1,
  },
  'conditions': [
  ['use_qt==1', {
    'includes': [
      'cdm.gypi',
      'device_event_log.gypi',
      'devtools_discovery.gypi',
      'devtools_http_handler.gypi',
      'display_compositor.gypi',
      'error_page.gypi',
      'keyed_service.gypi',
      'pref_registry.gypi',
      'user_prefs.gypi',
      'visitedlink.gypi',
      'web_cache.gypi',
      'webmessaging.gypi',
      'webusb.gypi',
    ],
    'conditions': [
      ['enable_basic_printing==1 or enable_print_preview==1', {
        'includes': [
          'printing.gypi',
        ],
      }],
      ['enable_pdf==1', {
        'includes': [
          'pdf.gypi',
        ],
      }],
      ]
  }, {
  'includes': [
    'about_handler.gypi',
    'auto_login_parser.gypi',
    'autofill.gypi',
    'base32.gypi',
    'bookmarks.gypi',
    'browser_sync.gypi',
    'browsing_data_ui.gypi',
    'bubble.gypi',
    'captive_portal.gypi',
    'cast_certificate.gypi',
    'certificate_reporting.gypi',
    'client_update_protocol.gypi',
    'cloud_devices.gypi',
    'component_updater.gypi',
    'content_settings.gypi',
    'cookie_config.gypi',
    'crash.gypi',
    'cronet.gypi',
    'crx_file.gypi',
    'data_reduction_proxy.gypi',
    'data_usage.gypi',
    'data_use_measurement.gypi',
    'device_event_log.gypi',
    'dom_distiller.gypi',
    'error_page.gypi',
    'favicon.gypi',
    'favicon_base.gypi',
    'flags_ui.gypi',
    'gcm_driver.gypi',
    'google.gypi',
    'guest_view.gypi',
    'handoff.gypi',
    'history.gypi',
    'image_fetcher.gypi',
    'infobars.gypi',
    'invalidation.gypi',
    'json_schema.gypi',
    'keyed_service.gypi',
    'language_usage_metrics.gypi',
    'leveldb_proto.gypi',
    'login.gypi',
    'memory_pressure.gypi',
    'metrics.gypi',
    'metrics_services_manager.gypi',
    'navigation_metrics.gypi',
    'net_log.gypi',
    'network_session_configurator.gypi',
    'network_time.gypi',
    'ntp_snippets.gypi',
    'ntp_tiles.gypi',
    'offline_pages.gypi',
    'omnibox.gypi',
    'onc.gypi',
    'open_from_clipboard.gypi',
    'os_crypt.gypi',
    'ownership.gypi',
    'password_manager.gypi',
    'plugins.gypi',
    'policy.gypi',
    'precache.gypi',
    'pref_registry.gypi',
    'profile_metrics.gypi',
    'proxy_config.gypi',
    'query_parser.gypi',
    'rappor.gypi',
    'search.gypi',
    'search_engines.gypi',
    'search_provider_logos.gypi',
    'security_interstitials.gypi',
    'security_state.gypi',
    'sessions.gypi',
    'signin.gypi',
    'ssl_config.gypi',
    'ssl_errors.gypi',
    'subresource_filter.gypi',
    'suggestions.gypi',
    'supervised_user_error_page.gypi',
    'sync_bookmarks.gypi',
    'sync_driver.gypi',
    'sync_sessions.gypi',
    'syncable_prefs.gypi',
    'toolbar.gypi',
    'translate.gypi',
    'undo.gypi',
    'update_client.gypi',
    'upload_list.gypi',
    'url_matcher.gypi',
    'user_prefs.gypi',
    'variations.gypi',
    'version_info.gypi',
    'version_ui.gypi',
    'web_resource.gypi',
    'web_restrictions.gypi',
    'webdata.gypi',
    'webdata_services.gypi',
  ],
  'conditions': [
    ['OS == "android"', {
      'includes': [
        'external_video_surface.gypi',
        'service_tab_launcher.gypi',
      ],
    }],
    ['OS != "ios"', {
      'includes': [
        'app_modal.gypi',
        'browsing_data.gypi',
        'cdm.gypi',
        'certificate_transparency.gypi',
        'contextual_search.gypi',
        'devtools_discovery.gypi',
        'devtools_http_handler.gypi',
        'display_compositor.gypi',
        'domain_reliability.gypi',
        'drive.gypi',
        'memory_coordinator.gypi',
        'navigation_interception.gypi',
        'network_hints.gypi',
        'packed_ct_ev_whitelist.gypi',
        'page_load_metrics.gypi',
        'power.gypi',
        'renderer_context_menu.gypi',
        'safe_browsing_db.gypi',
        'safe_json.gypi',
        'startup_metric_utils.gypi',
        'visitedlink.gypi',
        'wallpaper.gypi',
        'web_cache.gypi',
        'web_contents_delegate_android.gypi',
        'web_modal.gypi',
        'webmessaging.gypi',
        'webusb.gypi',
        'zoom.gypi',
      ],
    }],
    ['OS == "ios"', {
      'includes': [
        'webp_transcode.gypi',
      ],
    }],
    ['OS != "ios" and OS != "android"', {
      'includes': [
        'audio_modem.gypi',
        'chooser_controller.gypi',
        'copresence.gypi',
        'feedback.gypi',
        'proximity_auth.gypi',
        'storage_monitor.gypi',
      ]
    }],
    ['chromeos == 1', {
      'includes': [
        'arc.gypi',
        'pairing.gypi',
        'quirks.gypi',
        'timers.gypi',
        'wifi_sync.gypi',
      ],
    }],
    ['OS == "win" or OS == "mac"', {
      'includes': [
        'wifi.gypi',
      ],
    }],
    ['OS == "win"', {
      'includes': [
        'browser_watcher.gypi',
      ],
    }],
    ['chromeos == 1 or use_aura == 1', {
      'includes': [
        'session_manager.gypi',
        'user_manager.gypi',
      ],
    }],
    ['toolkit_views==1', {
      'includes': [
        'constrained_window.gypi',
      ],
    }],
    ['enable_basic_printing==1 or enable_print_preview==1', {
      'includes': [
        'printing.gypi',
      ],
    }],
    ['enable_plugins==1', {
      'includes': [
        'pdf.gypi',
      ],
    }],
    # TODO(tbarzic): Remove chromeos condition when there are non-chromeos apps
    # in components/apps.
    ['enable_extensions == 1 and chromeos == 1', {
      'includes': [
        'chrome_apps.gypi',
      ],
    }],
    ['enable_rlz_support==1', {
      'includes': [
        'rlz.gypi',
      ],
    }],
    ['use_ash==1', {
      'includes': [
        'exo.gypi',
      ],
    }],
  ],
  }],
],
}
