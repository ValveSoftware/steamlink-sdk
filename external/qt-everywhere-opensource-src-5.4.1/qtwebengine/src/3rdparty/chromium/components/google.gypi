# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'google_core_browser',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../ui/base/ui_base.gyp:ui_base',
        '../url/url.gyp:url_lib',
        'components_strings.gyp:components_strings',
        'keyed_service_core',
        'infobars_core',
        'url_fixer',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'google/core/browser/google_pref_names.cc',
        'google/core/browser/google_pref_names.h',
        'google/core/browser/google_search_metrics.cc',
        'google/core/browser/google_search_metrics.h',
        'google/core/browser/google_switches.cc',
        'google/core/browser/google_switches.h',
        'google/core/browser/google_url_tracker.cc',
        'google/core/browser/google_url_tracker.h',
        'google/core/browser/google_url_tracker_client.cc',
        'google/core/browser/google_url_tracker_client.h',
        'google/core/browser/google_url_tracker_infobar_delegate.cc',
        'google/core/browser/google_url_tracker_infobar_delegate.h',
        'google/core/browser/google_url_tracker_map_entry.cc',
        'google/core/browser/google_url_tracker_map_entry.h',
        'google/core/browser/google_url_tracker_navigation_helper.cc',
        'google/core/browser/google_url_tracker_navigation_helper.h',
        'google/core/browser/google_util.cc',
        'google/core/browser/google_util.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
  ],
}
