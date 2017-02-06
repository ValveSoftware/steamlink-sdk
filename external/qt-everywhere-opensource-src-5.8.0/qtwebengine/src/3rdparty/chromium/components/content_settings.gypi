# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/content_settings/core/browser
      'target_name': 'content_settings_core_browser',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
        'content_settings_core_common',
        'pref_registry',
        'prefs/prefs.gyp:prefs',
        'url_formatter/url_formatter.gyp:url_formatter',
      ],
      'variables': { 'enable_wexit_time_destructors': 1, },
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'content_settings/core/browser/content_settings_binary_value_map.cc',
        'content_settings/core/browser/content_settings_binary_value_map.h',
        'content_settings/core/browser/content_settings_client.h',
        'content_settings/core/browser/content_settings_default_provider.cc',
        'content_settings/core/browser/content_settings_default_provider.h',
        'content_settings/core/browser/content_settings_details.cc',
        'content_settings/core/browser/content_settings_details.h',
        'content_settings/core/browser/content_settings_info.cc',
        'content_settings/core/browser/content_settings_info.h',
        'content_settings/core/browser/content_settings_observable_provider.cc',
        'content_settings/core/browser/content_settings_observable_provider.h',
        'content_settings/core/browser/content_settings_observer.h',
        'content_settings/core/browser/content_settings_origin_identifier_value_map.cc',
        'content_settings/core/browser/content_settings_origin_identifier_value_map.h',
        'content_settings/core/browser/content_settings_policy_provider.cc',
        'content_settings/core/browser/content_settings_policy_provider.h',
        'content_settings/core/browser/content_settings_pref.cc',
        'content_settings/core/browser/content_settings_pref.h',
        'content_settings/core/browser/content_settings_pref_provider.cc',
        'content_settings/core/browser/content_settings_pref_provider.h',
        'content_settings/core/browser/content_settings_provider.h',
        'content_settings/core/browser/content_settings_registry.cc',
        'content_settings/core/browser/content_settings_registry.h',
        'content_settings/core/browser/content_settings_rule.cc',
        'content_settings/core/browser/content_settings_rule.h',
        'content_settings/core/browser/content_settings_usages_state.cc',
        'content_settings/core/browser/content_settings_usages_state.h',
        'content_settings/core/browser/content_settings_utils.cc',
        'content_settings/core/browser/content_settings_utils.h',
        'content_settings/core/browser/cookie_settings.cc',
        'content_settings/core/browser/cookie_settings.h',
        'content_settings/core/browser/host_content_settings_map.cc',
        'content_settings/core/browser/host_content_settings_map.h',
        'content_settings/core/browser/local_shared_objects_counter.h',
        'content_settings/core/browser/website_settings_info.cc',
        'content_settings/core/browser/website_settings_info.h',
        'content_settings/core/browser/website_settings_registry.cc',
        'content_settings/core/browser/website_settings_registry.h',
      ],
      'conditions': [
        ['enable_plugins == 1', {
          'sources': [
            'content_settings/core/browser/plugins_field_trial.cc',
            'content_settings/core/browser/plugins_field_trial.h',
          ],
        }],
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
    {
      # GN version: //components/content_settings/core/common
      'target_name': 'content_settings_core_common',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
      ],
      'variables': { 'enable_wexit_time_destructors': 1, },
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'content_settings/core/common/content_settings.cc',
        'content_settings/core/common/content_settings.h',
        'content_settings/core/common/content_settings_pattern.cc',
        'content_settings/core/common/content_settings_pattern.h',
        'content_settings/core/common/content_settings_pattern_parser.cc',
        'content_settings/core/common/content_settings_pattern_parser.h',
        'content_settings/core/common/content_settings_types.h',
        'content_settings/core/common/pref_names.cc',
        'content_settings/core/common/pref_names.h',
      ],
    },
    {
      # GN version: //components/content_settings/core/test:test_support
      'target_name': 'content_settings_core_test_support',
      'type': 'static_library',
      'dependencies': [
        'content_settings_core_browser',
        'content_settings_core_common',
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'content_settings/core/test/content_settings_test_utils.cc',
        'content_settings/core/test/content_settings_test_utils.h',
      ],
    },
  ],
  'conditions': [
    ['OS!="ios"', {
      'targets': [
        {
          # GN version: //components/content_settings/content/common
          'target_name': 'content_settings_content_common',
          'type': 'static_library',
          'dependencies': [
            'content_settings_core_common',
            '../base/base.gyp:base',
            '../ipc/ipc.gyp:ipc',
            '../url/url.gyp:url_lib',
            '../url/ipc/url_ipc.gyp:url_ipc',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            # Note: sources list duplicated in GN build.
            'content_settings/content/common/content_settings_message_generator.cc',
            'content_settings/content/common/content_settings_message_generator.h',
            'content_settings/content/common/content_settings_messages.h',
          ],
        },
      ],
    }]
  ],
}
