# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


{
  'targets': [
    {
      'target_name': 'data_reduction_proxy_browser',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../crypto/crypto.gyp:crypto',
        '../net/net.gyp:net',
        'data_reduction_proxy_common',
        'pref_registry',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'data_reduction_proxy/browser/data_reduction_proxy_auth_request_handler.cc',
        'data_reduction_proxy/browser/data_reduction_proxy_auth_request_handler.h',
        'data_reduction_proxy/browser/data_reduction_proxy_config_service.cc',
        'data_reduction_proxy/browser/data_reduction_proxy_config_service.h',
        'data_reduction_proxy/browser/data_reduction_proxy_configurator.h',
        'data_reduction_proxy/browser/data_reduction_proxy_metrics.cc',
        'data_reduction_proxy/browser/data_reduction_proxy_metrics.h',
        'data_reduction_proxy/browser/data_reduction_proxy_params.cc',
        'data_reduction_proxy/browser/data_reduction_proxy_params.h',
        'data_reduction_proxy/browser/data_reduction_proxy_prefs.cc',
        'data_reduction_proxy/browser/data_reduction_proxy_prefs.h',
        'data_reduction_proxy/browser/data_reduction_proxy_protocol.cc',
        'data_reduction_proxy/browser/data_reduction_proxy_protocol.h',
        'data_reduction_proxy/browser/data_reduction_proxy_settings.cc',
        'data_reduction_proxy/browser/data_reduction_proxy_settings.h',
        'data_reduction_proxy/browser/http_auth_handler_data_reduction_proxy.cc',
        'data_reduction_proxy/browser/http_auth_handler_data_reduction_proxy.h',
      ],
    },
    {
      'target_name': 'data_reduction_proxy_common',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'data_reduction_proxy/common/data_reduction_proxy_headers.cc',
        'data_reduction_proxy/common/data_reduction_proxy_headers.h',
        'data_reduction_proxy/common/data_reduction_proxy_pref_names.cc',
        'data_reduction_proxy/common/data_reduction_proxy_pref_names.h',
        'data_reduction_proxy/common/data_reduction_proxy_switches.cc',
        'data_reduction_proxy/common/data_reduction_proxy_switches.h',
      ],
    },
    {
      'target_name': 'data_reduction_proxy_test_support',
      'type': 'static_library',
      'dependencies' : [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../net/net.gyp:net_test_support',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'data_reduction_proxy_browser',
        'data_reduction_proxy_common',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'data_reduction_proxy/browser/data_reduction_proxy_settings_test_utils.cc',
        'data_reduction_proxy/browser/data_reduction_proxy_settings_test_utils.h',
      ],
    },
  ],
}

