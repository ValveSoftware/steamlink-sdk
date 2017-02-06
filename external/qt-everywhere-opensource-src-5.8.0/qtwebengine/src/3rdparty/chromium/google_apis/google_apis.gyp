# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,  # Use higher warning level.
  },
  'includes': [
    '../build/win_precompile.gypi',
  ],
  'targets': [
    {
      # GN version: //google_apis
      'target_name': 'google_apis',
      'type': 'static_library',
      'includes': [
        'determine_use_official_keys.gypi',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../crypto/crypto.gyp:crypto',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
      ],
      'conditions': [
        ['google_api_key!=""', {
          'defines': ['GOOGLE_API_KEY="<(google_api_key)"'],
        }],
        ['google_default_client_id!=""', {
          'defines': [
            'GOOGLE_DEFAULT_CLIENT_ID="<(google_default_client_id)"',
          ]
        }],
        ['google_default_client_secret!=""', {
          'defines': [
            'GOOGLE_DEFAULT_CLIENT_SECRET="<(google_default_client_secret)"',
          ]
        }],
        ['enable_extensions==1', {
          'sources': [
            # Note: sources list duplicated in GN build.
            'drive/auth_service.cc',
            'drive/auth_service.h',
            'drive/auth_service_interface.h',
            'drive/auth_service_observer.h',
            'drive/base_requests.cc',
            'drive/base_requests.h',
            'drive/drive_api_error_codes.cc',
            'drive/drive_api_error_codes.h',
            'drive/drive_api_parser.cc',
            'drive/drive_api_parser.h',
            'drive/drive_api_requests.cc',
            'drive/drive_api_requests.h',
            'drive/drive_api_url_generator.cc',
            'drive/drive_api_url_generator.h',
            'drive/drive_common_callbacks.h',
            'drive/files_list_request_runner.cc',
            'drive/files_list_request_runner.h',
            'drive/request_sender.cc',
            'drive/request_sender.h',
            'drive/request_util.cc',
            'drive/request_util.h',
            'drive/task_util.cc',
            'drive/task_util.h',
            'drive/time_util.cc',
            'drive/time_util.h',
          ],
        }],
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'gaia/account_tracker.cc',
        'gaia/account_tracker.h',
        'gaia/gaia_auth_consumer.cc',
        'gaia/gaia_auth_consumer.h',
        'gaia/gaia_auth_fetcher.cc',
        'gaia/gaia_auth_fetcher.h',
        'gaia/gaia_auth_util.cc',
        'gaia/gaia_auth_util.h',
        'gaia/gaia_constants.cc',
        'gaia/gaia_constants.h',
        'gaia/gaia_oauth_client.cc',
        'gaia/gaia_oauth_client.h',
        'gaia/gaia_switches.cc',
        'gaia/gaia_switches.h',
        'gaia/gaia_urls.cc',
        'gaia/gaia_urls.h',
        'gaia/google_service_auth_error.cc',
        'gaia/google_service_auth_error.h',
        'gaia/identity_provider.cc',
        'gaia/identity_provider.h',
        'gaia/oauth2_access_token_consumer.h',
        'gaia/oauth2_access_token_fetcher.cc',
        'gaia/oauth2_access_token_fetcher.h',
        'gaia/oauth2_access_token_fetcher_impl.cc',
        'gaia/oauth2_access_token_fetcher_impl.h',
        'gaia/oauth2_access_token_fetcher_immediate_error.cc',
        'gaia/oauth2_access_token_fetcher_immediate_error.h',
        'gaia/oauth2_api_call_flow.cc',
        'gaia/oauth2_api_call_flow.h',
        'gaia/oauth2_mint_token_flow.cc',
        'gaia/oauth2_mint_token_flow.h',
        'gaia/oauth2_token_service.cc',
        'gaia/oauth2_token_service.h',
        'gaia/oauth2_token_service_delegate.cc',
        'gaia/oauth2_token_service_delegate.h',
        'gaia/oauth2_token_service_request.cc',
        'gaia/oauth2_token_service_request.h',
        'gaia/oauth_request_signer.cc',
        'gaia/oauth_request_signer.h',
        'gaia/ubertoken_fetcher.cc',
        'gaia/ubertoken_fetcher.h',
        'google_api_keys.cc',
        'google_api_keys.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
    {
      'target_name': 'google_apis_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:run_all_unittests',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'google_apis',
        'google_apis_test_support',
      ],
      'includes': [
        'determine_use_official_keys.gypi',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'gaia/account_tracker_unittest.cc',
        'gaia/gaia_auth_fetcher_unittest.cc',
        'gaia/gaia_auth_util_unittest.cc',
        'gaia/gaia_oauth_client_unittest.cc',
        'gaia/google_service_auth_error_unittest.cc',
        'gaia/oauth2_access_token_fetcher_impl_unittest.cc',
        'gaia/oauth2_api_call_flow_unittest.cc',
        'gaia/oauth2_mint_token_flow_unittest.cc',
        'gaia/oauth2_token_service_request_unittest.cc',
        'gaia/oauth2_token_service_unittest.cc',
        'gaia/oauth_request_signer_unittest.cc',
        'gaia/ubertoken_fetcher_unittest.cc',
        'google_api_keys_unittest.cc',
      ],
      'conditions': [
        ['enable_extensions==1', {
          'sources': [
            'drive/base_requests_server_unittest.cc',
            'drive/base_requests_unittest.cc',
            'drive/drive_api_parser_unittest.cc',
            'drive/drive_api_requests_unittest.cc',
            'drive/drive_api_url_generator_unittest.cc',
            'drive/files_list_request_runner_unittest.cc',
            'drive/request_sender_unittest.cc',
            'drive/request_util_unittest.cc',
            'drive/time_util_unittest.cc',
          ],
        }],
      ],
    },
    {
      # GN version: //google_apis:test_support
      'target_name': 'google_apis_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../net/net.gyp:net',
        '../net/net.gyp:net_test_support',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../net/net.gyp:net',
        '../net/net.gyp:net_test_support',
      ],
      'sources': [
        'gaia/fake_gaia.cc',
        'gaia/fake_gaia.h',
        'gaia/fake_identity_provider.cc',
        'gaia/fake_identity_provider.h',
        'gaia/fake_oauth2_token_service_delegate.cc',
        'gaia/fake_oauth2_token_service_delegate.h',
        'gaia/fake_oauth2_token_service.cc',
        'gaia/fake_oauth2_token_service.h',
        'gaia/mock_url_fetcher_factory.h',
        'gaia/oauth2_token_service_test_util.cc',
        'gaia/oauth2_token_service_test_util.h',
      ],
      'conditions': [
        ['enable_extensions==1', {
          'sources': [
            'drive/dummy_auth_service.cc',
            'drive/dummy_auth_service.h',
            'drive/test_util.cc',
            'drive/test_util.h',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'google_apis_unittests_run',
          'type': 'none',
          'dependencies': [
            'google_apis_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'google_apis_unittests.isolate',
          ],
        },
      ],
    }],
  ],
}
