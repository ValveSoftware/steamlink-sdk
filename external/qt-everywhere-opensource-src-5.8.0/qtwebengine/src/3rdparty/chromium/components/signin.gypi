# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/signin/core/common
      'target_name': 'signin_core_common',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'sources': [
        'signin/core/common/profile_management_switches.cc',
        'signin/core/common/profile_management_switches.h',
        'signin/core/common/signin_pref_names.cc',
        'signin/core/common/signin_pref_names.h',
        'signin/core/common/signin_switches.cc',
        'signin/core/common/signin_switches.h',
      ],
    },
    {
      # GN version: //components/signin/core/account_id
      'target_name': 'signin_core_account_id',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'sources': [
        'signin/core/account_id/account_id.cc',
        'signin/core/account_id/account_id.h',
      ],
    },
    {
      # GN version: //components/signin/core/browser
      'target_name': 'signin_core_browser',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../crypto/crypto.gyp:crypto',
        '../google_apis/google_apis.gyp:google_apis',
        '../net/net.gyp:net',
        '../sql/sql.gyp:sql',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_proto_cpp',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        'content_settings_core_browser',
        'content_settings_core_common',
        'google_core_browser',
        'invalidation_public',
        'keyed_service_core',
        'metrics',
        'os_crypt',
        'prefs/prefs.gyp:prefs',
        'signin_core_common',
        'signin_core_account_id',
        'webdata_common',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'signin/core/browser/about_signin_internals.cc',
        'signin/core/browser/about_signin_internals.h',
        'signin/core/browser/account_fetcher_service.cc',
        'signin/core/browser/account_fetcher_service.h',
        'signin/core/browser/account_info.cc',
        'signin/core/browser/account_info.h',
        'signin/core/browser/account_info_fetcher.cc',
        'signin/core/browser/account_info_fetcher.h',
        'signin/core/browser/account_investigator.cc',
        'signin/core/browser/account_investigator.h',
        'signin/core/browser/account_reconcilor.cc',
        'signin/core/browser/account_reconcilor.h',
        'signin/core/browser/account_tracker_service.cc',
        'signin/core/browser/account_tracker_service.h',
        'signin/core/browser/android/component_jni_registrar.cc',
        'signin/core/browser/android/component_jni_registrar.h',
        'signin/core/browser/child_account_info_fetcher.cc',
        'signin/core/browser/child_account_info_fetcher.h',
        'signin/core/browser/child_account_info_fetcher_android.cc',
        'signin/core/browser/child_account_info_fetcher_android.h',
        'signin/core/browser/child_account_info_fetcher_impl.cc',
        'signin/core/browser/child_account_info_fetcher_impl.h',
        'signin/core/browser/device_activity_fetcher.cc',
        'signin/core/browser/device_activity_fetcher.h',
        'signin/core/browser/gaia_cookie_manager_service.cc',
        'signin/core/browser/gaia_cookie_manager_service.h',
        'signin/core/browser/profile_identity_provider.cc',
        'signin/core/browser/profile_identity_provider.h',
        'signin/core/browser/profile_oauth2_token_service.cc',
        'signin/core/browser/profile_oauth2_token_service.h',
        'signin/core/browser/refresh_token_annotation_request.cc',
        'signin/core/browser/refresh_token_annotation_request.h',
        'signin/core/browser/signin_client.cc',
        'signin/core/browser/signin_client.h',
        'signin/core/browser/signin_cookie_changed_subscription.cc',
        'signin/core/browser/signin_cookie_changed_subscription.h',
        'signin/core/browser/signin_error_controller.cc',
        'signin/core/browser/signin_error_controller.h',
        'signin/core/browser/signin_header_helper.cc',
        'signin/core/browser/signin_header_helper.h',
        'signin/core/browser/signin_internals_util.cc',
        'signin/core/browser/signin_internals_util.h',
        'signin/core/browser/signin_investigator.cc',
        'signin/core/browser/signin_investigator.h',
        'signin/core/browser/signin_manager.cc',
        'signin/core/browser/signin_manager.h',
        'signin/core/browser/signin_manager_base.cc',
        'signin/core/browser/signin_manager_base.h',
        'signin/core/browser/signin_metrics.cc',
        'signin/core/browser/signin_metrics.h',
        'signin/core/browser/signin_status_metrics_provider.cc',
        'signin/core/browser/signin_status_metrics_provider.h',
        'signin/core/browser/signin_status_metrics_provider_base.cc',
        'signin/core/browser/signin_status_metrics_provider_base.h',
        'signin/core/browser/signin_status_metrics_provider_delegate.cc',
        'signin/core/browser/signin_status_metrics_provider_delegate.h',
        'signin/core/browser/signin_tracker.cc',
        'signin/core/browser/signin_tracker.h',
        'signin/core/browser/webdata/token_service_table.cc',
        'signin/core/browser/webdata/token_service_table.h',
        'signin/core/browser/webdata/token_web_data.cc',
        'signin/core/browser/webdata/token_web_data.h',
      ],
      'conditions': [
        ['OS=="android"', {
          'dependencies': [
            'signin_core_browser_jni_headers',
          ],
          'sources!': [
            'signin/core/browser/child_account_info_fetcher_impl.cc',
            'signin/core/browser/child_account_info_fetcher_impl.h',
          ],
        }],
        ['chromeos==1', {
          'sources!': [
            'signin/core/browser/signin_manager.cc',
            'signin/core/browser/signin_status_metrics_provider.cc',
            'signin/core/browser/signin_status_metrics_provider_delegate.cc',
          ],
        }],
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
    {
      # GN version: //components/signin/core/browser:test_support
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
        # Note: file list duplicated in GN build.
        'signin/core/browser/fake_account_fetcher_service.cc',
        'signin/core/browser/fake_account_fetcher_service.h',
        'signin/core/browser/fake_auth_status_provider.cc',
        'signin/core/browser/fake_auth_status_provider.h',
        'signin/core/browser/fake_gaia_cookie_manager_service.cc',
        'signin/core/browser/fake_gaia_cookie_manager_service.h',
        'signin/core/browser/fake_profile_oauth2_token_service.cc',
        'signin/core/browser/fake_profile_oauth2_token_service.h',
        'signin/core/browser/fake_signin_manager.cc',
        'signin/core/browser/fake_signin_manager.h',
        'signin/core/browser/test_signin_client.cc',
        'signin/core/browser/test_signin_client.h',
      ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          # GN version: //components/signin/core/browser/android:java
          'target_name': 'signin_core_browser_java',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base',
            '../sync/sync.gyp:sync_java',
          ],
          'variables': {
            'java_in_dir': 'signin/core/browser/android/java',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          # GN version: //components/signin/core/browser/android:jni_headers
          'target_name': 'signin_core_browser_jni_headers',
          'type': 'none',
          'sources': [
            'signin/core/browser/android/java/src/org/chromium/components/signin/ChildAccountInfoFetcher.java',
          ],
          'variables': {
            'jni_gen_package': 'components/signin',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          # GN: //components/signin/core/browser:investigated_scenario_java
          'target_name': 'investigated_scenario_java',
          'type': 'none',
          'variables': {
            'source_file': 'signin/core/browser/signin_investigator.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
      ],
    }],
    ['OS == "ios"', {
      'targets': [
        {
          # GN version: //components/signin/ios/browser
          'target_name': 'signin_ios_browser',
          'type': 'static_library',
          'dependencies': [
            '../ios/web/ios_web.gyp:ios_web',
            'signin_core_browser',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            # Note: file list duplicated in GN build.
            'signin/ios/browser/account_consistency_service.h',
            'signin/ios/browser/account_consistency_service.mm',
            'signin/ios/browser/manage_accounts_delegate.h',
            'signin/ios/browser/merge_session_observer_bridge.h',
            'signin/ios/browser/merge_session_observer_bridge.mm',
            'signin/ios/browser/oauth2_token_service_observer_bridge.h',
            'signin/ios/browser/oauth2_token_service_observer_bridge.mm',
            'signin/ios/browser/profile_oauth2_token_service_ios_delegate.h',
            'signin/ios/browser/profile_oauth2_token_service_ios_delegate.mm',
            'signin/ios/browser/profile_oauth2_token_service_ios_provider.h',
            'signin/ios/browser/profile_oauth2_token_service_ios_provider.mm',
          ],
        },
        {
          # GN version: //components/signin/ios/browser:test_support
          'target_name': 'signin_ios_browser_test_support',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../google_apis/google_apis.gyp:google_apis_test_support',
            'signin_ios_browser',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            # Note: file list duplicated in GN build.
            'signin/ios/browser/fake_profile_oauth2_token_service_ios_delegate.h',
            'signin/ios/browser/fake_profile_oauth2_token_service_ios_delegate.mm',
            'signin/ios/browser/fake_profile_oauth2_token_service_ios_provider.h',
            'signin/ios/browser/fake_profile_oauth2_token_service_ios_provider.mm',
          ],
        },
      ],
    }],
  ],
}
