# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/security_interstitials/core
      'target_name': 'security_interstitials_core',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../net/net.gyp:net',
        '../ui/base/ui_base.gyp:ui_base',
        'components_strings.gyp:components_strings',
        'google_core_browser',
        'history_core_browser',
        'metrics',
        'rappor',
        'ssl_errors',
        'url_formatter/url_formatter.gyp:url_formatter'
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'security_interstitials/core/bad_clock_ui.cc',
        'security_interstitials/core/bad_clock_ui.h',
        'security_interstitials/core/common_string_util.cc',
        'security_interstitials/core/common_string_util.h',
        'security_interstitials/core/controller_client.cc',
        'security_interstitials/core/controller_client.h',
        'security_interstitials/core/metrics_helper.cc',
        'security_interstitials/core/metrics_helper.h',
        'security_interstitials/core/ssl_error_ui.cc',
        'security_interstitials/core/ssl_error_ui.h',
      ]
    }
  ]
}