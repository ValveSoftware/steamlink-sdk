# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'signin_core_common',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'sources': [
        'signin/core/common/signin_pref_names.cc',
        'signin/core/common/signin_pref_names.h',
        'signin/core/common/signin_switches.cc',
        'signin/core/common/signin_switches.h',
        'signin/core/common/profile_management_switches.cc',
        'signin/core/common/profile_management_switches.h',
      ],
    },
    {
      'target_name': 'signin_core_browser',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../crypto/crypto.gyp:crypto',
        '../google_apis/google_apis.gyp:google_apis',
        '../net/net.gyp:net',
        '../sql/sql.gyp:sql',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        'keyed_service_core',
        'os_crypt',
        'signin_core_common',
        'webdata_common',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'signin/core/browser/about_signin_internals.cc',
        'signin/core/browser/about_signin_internals.h',
        'signin/core/browser/account_reconcilor.cc',
        'signin/core/browser/account_reconcilor.h',
        'signin/core/browser/account_service_flag_fetcher.cc',
        'signin/core/browser/account_service_flag_fetcher.h',
        'signin/core/browser/mutable_profile_oauth2_token_service.cc',
        'signin/core/browser/mutable_profile_oauth2_token_service.h',
        'signin/core/browser/profile_oauth2_token_service.cc',
        'signin/core/browser/profile_oauth2_token_service.h',
        'signin/core/browser/signin_account_id_helper.cc',
        'signin/core/browser/signin_account_id_helper.h',
        'signin/core/browser/signin_client.h',
        'signin/core/browser/signin_error_controller.cc',
        'signin/core/browser/signin_error_controller.h',
        'signin/core/browser/signin_internals_util.cc',
        'signin/core/browser/signin_internals_util.h',
        'signin/core/browser/signin_manager_base.cc',
        'signin/core/browser/signin_manager_base.h',
        'signin/core/browser/signin_manager.cc',
        'signin/core/browser/signin_manager.h',
        'signin/core/browser/signin_manager_cookie_helper.cc',
        'signin/core/browser/signin_manager_cookie_helper.h',
        'signin/core/browser/signin_metrics.cc',
        'signin/core/browser/signin_metrics.h',
        'signin/core/browser/signin_oauth_helper.cc',
        'signin/core/browser/signin_oauth_helper.h',
        'signin/core/browser/signin_tracker.cc',
        'signin/core/browser/signin_tracker.h',
        'signin/core/browser/webdata/token_service_table.cc',
        'signin/core/browser/webdata/token_service_table.h',
        'signin/core/browser/webdata/token_web_data.cc',
        'signin/core/browser/webdata/token_web_data.h',
      ],
      'conditions': [
        ['OS=="android"', {
          'sources!': [
            # Not used on Android.
            'signin/core/browser/mutable_profile_oauth2_token_service.cc',
            'signin/core/browser/mutable_profile_oauth2_token_service.h',
          ],
        }],
        ['chromeos==1', {
          'sources!': [
            'signin/core/browser/signin_manager.cc',
          ],
        }],
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
    {
      'target_name': 'signin_core_browser_test_support',
      'type': 'static_library',
      'dependencies': [
        'signin_core_browser',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'signin/core/browser/fake_auth_status_provider.cc',
        'signin/core/browser/fake_auth_status_provider.h',
        'signin/core/browser/test_signin_client.cc',
        'signin/core/browser/test_signin_client.h',
      ],
    },
  ],
  'conditions': [
    ['OS == "ios"', {
      'targets': [
        {
          'target_name': 'signin_ios_browser',
          'type': 'static_library',
          'dependencies': [
            'signin_core_browser',
            '../ios/provider/ios_components.gyp:ios_components',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'signin/ios/browser/profile_oauth2_token_service_ios.h',
            'signin/ios/browser/profile_oauth2_token_service_ios.mm',
          ],
        },
      ],
    }],
  ],
}
