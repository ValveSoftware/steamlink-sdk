# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/browser_sync_browser
      'target_name': 'browser_sync_browser',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../google_apis/google_apis.gyp:google_apis',
        '../net/net.gyp:net',
        '../sync/sync.gyp:sync',
        '../ui/base/ui_base.gyp:ui_base',
        'autofill_core_browser',
        'autofill_core_common',
        'browser_sync_common',
        'components_strings.gyp:components_strings',
        'dom_distiller_core',
        'generate_version_info',
        'history_core_browser',
        'invalidation_impl',
        'invalidation_public',
        'keyed_service_core',
        'password_manager_core_browser',
        'password_manager_sync_browser',
        'pref_registry',
        'signin_core_browser',
        'syncable_prefs',
        'sync_bookmarks',
        'sync_driver',
        'sync_sessions',
        'variations',
        'version_info',
      ],
      'include_dirs': [
        '..',
      ],
      'export_dependent_settings': [
        'sync_driver',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'browser_sync/browser/profile_sync_components_factory_impl.cc',
        'browser_sync/browser/profile_sync_components_factory_impl.h',
        'browser_sync/browser/profile_sync_service.cc',
        'browser_sync/browser/profile_sync_service.h',
        'browser_sync/browser/signin_confirmation_helper.cc',
        'browser_sync/browser/signin_confirmation_helper.h',
      ],
    },
    {
      # GN version: //components/browser_sync_common
      'target_name': 'browser_sync_common',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'browser_sync/common/browser_sync_switches.cc',
        'browser_sync/common/browser_sync_switches.h',
      ],
    },
    {
      # GN version: //components/browser_sync/browser:test_support
      'target_name': 'browser_sync_browser_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../google_apis/google_apis.gyp:google_apis',
        '../sync/sync.gyp:sync',
        '../testing/gmock.gyp:gmock',
        'bookmarks_browser',
        'browser_sync_browser',
        'history_core_browser',
        'invalidation_impl',
        'invalidation_test_support',
        'pref_registry',
        'signin_core_browser',
        'signin_core_browser_test_support',
        'sync_driver',
        'sync_driver_test_support',
        'sync_sessions_test_support',
        'syncable_prefs_test_support',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'browser_sync/browser/abstract_profile_sync_service_test.cc',
        'browser_sync/browser/abstract_profile_sync_service_test.h',
        'browser_sync/browser/profile_sync_service_mock.cc',
        'browser_sync/browser/profile_sync_service_mock.h',
        'browser_sync/browser/profile_sync_test_util.cc',
        'browser_sync/browser/profile_sync_test_util.h',
        'browser_sync/browser/test_http_bridge_factory.cc',
        'browser_sync/browser/test_http_bridge_factory.h',
        'browser_sync/browser/test_profile_sync_service.cc',
        'browser_sync/browser/test_profile_sync_service.h',
      ],
    }
  ],
}
