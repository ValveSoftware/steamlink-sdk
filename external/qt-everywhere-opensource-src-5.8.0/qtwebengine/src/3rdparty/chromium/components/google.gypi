# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/google/core/browser
      'target_name': 'google_core_browser',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
        'components_strings.gyp:components_strings',
        'data_use_measurement_core',
        'keyed_service_core',
        'pref_registry',
        'url_formatter/url_formatter.gyp:url_formatter',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources duplicated in GN build.
        'google/core/browser/google_pref_names.cc',
        'google/core/browser/google_pref_names.h',
        'google/core/browser/google_switches.cc',
        'google/core/browser/google_switches.h',
        'google/core/browser/google_url_tracker.cc',
        'google/core/browser/google_url_tracker.h',
        'google/core/browser/google_url_tracker_client.cc',
        'google/core/browser/google_url_tracker_client.h',
        'google/core/browser/google_util.cc',
        'google/core/browser/google_util.h',
      ],
    },
  ],
}
