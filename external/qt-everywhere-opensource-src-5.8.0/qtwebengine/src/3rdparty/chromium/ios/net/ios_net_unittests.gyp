# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ios/net:ios_net_unittests
      'target_name': 'ios_net_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:run_all_unittests',
        '../../net/net.gyp:net_test_support',
        '../../testing/gtest.gyp:gtest',
        '../../url/url.gyp:url_lib',
        'ios_net.gyp:ios_net',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'clients/crn_forwarding_network_client_factory_unittest.mm',
        'cookies/cookie_cache_unittest.cc',
        'cookies/cookie_creation_time_manager_unittest.mm',
        'cookies/cookie_store_ios_unittest.mm',
        'cookies/system_cookie_util_unittest.mm',
        'http_response_headers_util_unittest.mm',
        'nsurlrequest_util_unittest.mm',
        'protocol_handler_util_unittest.mm',
        'url_scheme_util_unittest.mm',
      ],
    },
  ],
}
