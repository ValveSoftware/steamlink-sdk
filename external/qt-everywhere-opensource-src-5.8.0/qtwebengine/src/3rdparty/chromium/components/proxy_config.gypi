# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/proxy_config
      'target_name': 'proxy_config',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
        'pref_registry',
        'prefs/prefs.gyp:prefs',
      ],
      'include_dirs': [
        '..',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
      'defines': [
        'PROXY_CONFIG_IMPLEMENTATION',
      ],
      'sources': [
        'proxy_config/pref_proxy_config_tracker.cc',
        'proxy_config/pref_proxy_config_tracker.h',
        'proxy_config/pref_proxy_config_tracker_impl.cc',
        'proxy_config/pref_proxy_config_tracker_impl.h',
        'proxy_config/proxy_config_dictionary.cc',
        'proxy_config/proxy_config_dictionary.h',
        'proxy_config/proxy_config_export.h',
        'proxy_config/proxy_config_pref_names.cc',
        'proxy_config/proxy_config_pref_names.h',
        'proxy_config/proxy_prefs.cc',
        'proxy_config/proxy_prefs.h',
      ],
    },
  ],
}
