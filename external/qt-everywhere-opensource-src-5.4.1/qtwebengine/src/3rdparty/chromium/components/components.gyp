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
      'visitedlink.gypi',
    ],
  }, {
  'includes': [
    'auto_login_parser.gypi',
    'autocomplete.gypi',
    'autofill.gypi',
    'bookmarks.gypi',
    'breakpad.gypi',
    'captive_portal.gypi',
    'cloud_devices.gypi',
    'cronet.gypi',
    'data_reduction_proxy.gypi',
    'dom_distiller.gypi',
    'domain_reliability.gypi',
    'enhanced_bookmarks.gypi',
    'favicon.gypi',
    'favicon_base.gypi',
    'google.gypi',
    'history.gypi',
    'infobars.gypi',
    'json_schema.gypi',
    'keyed_service.gypi',
    'language_usage_metrics.gypi',
    'leveldb_proto.gypi',
    'metrics.gypi',
    'navigation_metrics.gypi',
    'network_time.gypi',
    'onc.gypi',
    'os_crypt.gypi',
    'password_manager.gypi',
    'policy.gypi',
    'precache.gypi',
    'pref_registry.gypi',
    'query_parser.gypi',
    'rappor.gypi',
    'search_engines.gypi',
    'search_provider_logos.gypi',
    'signin.gypi',
    'startup_metric_utils.gypi',
    'translate.gypi',
    'url_fixer.gypi',
    'url_matcher.gypi',
    'user_prefs.gypi',
    'variations.gypi',
    'webdata.gypi',
  ],
  'conditions': [
    ['OS != "ios"', {
      'includes': [
        'cdm.gypi',
        'navigation_interception.gypi',
        'plugins.gypi',
        'sessions.gypi',
        'visitedlink.gypi',
        'web_contents_delegate_android.gypi',
        'web_modal.gypi',
      ],
    }],
    ['OS != "android"', {
      'includes': [
        'feedback.gypi',
      ]
    }],
    ['OS != "ios" and OS != "android"', {
      'includes': [
        'storage_monitor.gypi',
        'usb_service.gypi',
      ]
    }],
    ['OS == "win" or OS == "mac"', {
      'includes': [
        'wifi.gypi',
      ],
    }],
    ['android_webview_build == 0', {
      # Android WebView fails to build if a dependency on these targets is
      # introduced.
      'includes': [
        'gcm_driver.gypi',
        'sync_driver.gypi',
        'invalidation.gypi',
      ],
    }],
  ],
  }],
],
}
