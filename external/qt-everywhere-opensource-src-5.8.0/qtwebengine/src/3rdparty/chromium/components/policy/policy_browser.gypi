# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'dependencies': [
    '../base/base.gyp:base',
    '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
    '../net/net.gyp:net',
    '../ui/base/ui_base.gyp:ui_base',
    '../url/url.gyp:url_lib',
    'bookmarks_browser',
    'bookmarks_managed',
    'components_strings.gyp:components_strings',
    'keyed_service_core',
    'pref_registry',
    'prefs/prefs.gyp:prefs',
    'url_matcher',
  ],
  'defines': [
    'POLICY_COMPONENT_IMPLEMENTATION',
  ],
  'include_dirs': [
    '..',
  ],
  'sources': [
    # Note that these sources are always included, even for builds that
    # disable policy. Most source files should go in the conditional
    # sources list below.
    # url_blacklist_manager.h is used by managed mode.
    'core/browser/url_blacklist_manager.cc',
    'core/browser/url_blacklist_manager.h',
  ],
  'conditions': [
    # GN version: //components/policy/core/browser
    ['configuration_policy==1', {
      'dependencies': [
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        'autofill_core_common',
        'cloud_policy_proto',
        'policy',
        'proxy_config',
      ],
      'sources': [
        'core/browser/autofill_policy_handler.cc',
        'core/browser/autofill_policy_handler.h',
        'core/browser/browser_policy_connector.cc',
        'core/browser/browser_policy_connector.h',
        'core/browser/browser_policy_connector_base.cc',
        'core/browser/browser_policy_connector_base.h',
        'core/browser/browser_policy_connector_ios.h',
        'core/browser/browser_policy_connector_ios.mm',
        'core/browser/cloud/message_util.cc',
        'core/browser/cloud/message_util.h',
        'core/browser/configuration_policy_handler.cc',
        'core/browser/configuration_policy_handler.h',
        'core/browser/configuration_policy_handler_list.cc',
        'core/browser/configuration_policy_handler_list.h',
        'core/browser/configuration_policy_pref_store.cc',
        'core/browser/configuration_policy_pref_store.h',
        'core/browser/policy_error_map.cc',
        'core/browser/policy_error_map.h',
        'core/browser/proxy_policy_handler.cc',
        'core/browser/proxy_policy_handler.h',
        'core/browser/url_blacklist_policy_handler.cc',
        'core/browser/url_blacklist_policy_handler.h',
      ],
      'conditions': [
        ['OS=="android"', {
          'sources': [
            'core/browser/android/android_combined_policy_provider.cc',
            'core/browser/android/android_combined_policy_provider.h',
            'core/browser/android/component_jni_registrar.cc',
            'core/browser/android/component_jni_registrar.h',
            'core/browser/android/policy_converter.cc',
            'core/browser/android/policy_converter.h',
          ],
          'dependencies': [ 'policy_jni_headers' ]
        }]
      ]
    }],
  ],
}
