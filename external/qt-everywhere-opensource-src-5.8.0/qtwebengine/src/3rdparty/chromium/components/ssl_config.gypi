# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/ssl_config
      'target_name': 'ssl_config',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        'content_settings_core_browser',
        'content_settings_core_common',
        'prefs/prefs.gyp:prefs',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'ssl_config/ssl_config_prefs.cc',
        'ssl_config/ssl_config_prefs.h',
        'ssl_config/ssl_config_service_manager.h',
        'ssl_config/ssl_config_service_manager_pref.cc',
        'ssl_config/ssl_config_switches.cc',
        'ssl_config/ssl_config_switches.h',
      ],
    },
  ],
}
