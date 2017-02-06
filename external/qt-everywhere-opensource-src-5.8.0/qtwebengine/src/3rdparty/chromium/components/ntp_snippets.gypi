# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/ntp_snippets
      'target_name': 'ntp_snippets',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../google_apis/google_apis.gyp:google_apis',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
        '../third_party/icu/icu.gyp:icuuc',
        'data_use_measurement_core',
        'image_fetcher',
        'keyed_service_core',
        'leveldb_proto',
        'prefs/prefs.gyp:prefs',
        'signin_core_browser',
        'suggestions',
        'sync_driver',
        'variations',
        'variations_net',
      ],
      'sources': [
        'ntp_snippets/content_suggestion_category.h',
        'ntp_snippets/content_suggestion.cc',
        'ntp_snippets/content_suggestion.h',
        'ntp_snippets/content_suggestions_provider_type.h',
        'ntp_snippets/ntp_snippet.cc',
        'ntp_snippets/ntp_snippet.h',
        'ntp_snippets/ntp_snippets_constants.cc',
        'ntp_snippets/ntp_snippets_constants.h',
        'ntp_snippets/ntp_snippets_database.cc',
        'ntp_snippets/ntp_snippets_database.h',
        'ntp_snippets/ntp_snippets_fetcher.cc',
        'ntp_snippets/ntp_snippets_fetcher.h',
        'ntp_snippets/ntp_snippets_scheduler.h',
        'ntp_snippets/ntp_snippets_service.cc',
        'ntp_snippets/ntp_snippets_service.h',
        'ntp_snippets/ntp_snippets_status_service.cc',
        'ntp_snippets/ntp_snippets_status_service.h',
        'ntp_snippets/pref_names.cc',
        'ntp_snippets/pref_names.h',
        'ntp_snippets/proto/ntp_snippets.proto',
        'ntp_snippets/switches.cc',
        'ntp_snippets/switches.h',
      ],
      'variables': {
        'proto_in_dir': 'ntp_snippets/proto',
        'proto_out_dir': 'components/ntp_snippets/proto',
      },
      'includes': [ '../build/protoc.gypi' ],
    },
  ],
}
