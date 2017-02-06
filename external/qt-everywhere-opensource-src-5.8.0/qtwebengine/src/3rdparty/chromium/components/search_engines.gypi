# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/search_engines
      'target_name': 'search_engines',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../google_apis/google_apis.gyp:google_apis',
        '../net/net.gyp:net',
        '../sql/sql.gyp:sql',
        '../sync/sync.gyp:sync',
        '../third_party/libxml/libxml.gyp:libxml',
        '../ui/gfx/gfx.gyp:gfx',
        '../url/url.gyp:url_lib',
        'component_metrics_proto',
        'components_strings.gyp:components_strings',
        'components.gyp:infobars_core',
        'google_core_browser',
        'history_core_browser',
        'keyed_service_core',
        'policy',
        'pref_registry',
        'prefs/prefs.gyp:prefs',
        'rappor',
        'search_engines/prepopulated_engines.gyp:prepopulated_engines',
        'sync_driver',
        'url_formatter/url_formatter.gyp:url_formatter',
        'webdata_common',
      ],
      'export_dependent_settings': [
        'component_metrics_proto',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'search_engines/default_search_manager.cc',
        'search_engines/default_search_manager.h',
        'search_engines/default_search_policy_handler.cc',
        'search_engines/default_search_policy_handler.h',
        'search_engines/default_search_pref_migration.cc',
        'search_engines/default_search_pref_migration.h',
        'search_engines/keyword_table.cc',
        'search_engines/keyword_table.h',
        'search_engines/keyword_web_data_service.cc',
        'search_engines/keyword_web_data_service.h',
        'search_engines/search_engine_data_type_controller.cc',
        'search_engines/search_engine_data_type_controller.h',
        'search_engines/search_engine_type.h',
        'search_engines/search_engines_pref_names.cc',
        'search_engines/search_engines_pref_names.h',
        'search_engines/search_engines_switches.cc',
        'search_engines/search_engines_switches.h',
        'search_engines/search_host_to_urls_map.cc',
        'search_engines/search_host_to_urls_map.h',
        'search_engines/search_terms_data.cc',
        'search_engines/search_terms_data.h',
        'search_engines/template_url.cc',
        'search_engines/template_url.h',
        'search_engines/template_url_data.cc',
        'search_engines/template_url_data.h',
        'search_engines/template_url_fetcher.cc',
        'search_engines/template_url_fetcher.h',
        'search_engines/template_url_id.h',
        'search_engines/template_url_parser.cc',
        'search_engines/template_url_parser.h',
        'search_engines/template_url_prepopulate_data.cc',
        'search_engines/template_url_prepopulate_data.h',
        'search_engines/template_url_service.cc',
        'search_engines/template_url_service.h',
        'search_engines/template_url_service_client.h',
        'search_engines/template_url_service_observer.h',
        'search_engines/util.cc',
        'search_engines/util.h',
      ],
      'conditions': [
        ['configuration_policy==0', {
          'dependencies!': [
            'policy'
           ],
           'sources!': [
             'search_engines/default_search_policy_handler.cc',
             'search_engines/default_search_policy_handler.h',
           ],
        }],
      ],
    },
    {
      # GN version: //components/search_engines:test_support
      'target_name': 'search_engines_test_support',
      'type': 'static_library',
      'dependencies': [
        '../testing/gtest.gyp:gtest',
        'search_engines',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'search_engines/default_search_pref_test_util.cc',
        'search_engines/default_search_pref_test_util.h',
        'search_engines/testing_search_terms_data.cc',
        'search_engines/testing_search_terms_data.h',
      ],
    },
  ],
}
