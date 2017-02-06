# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'search',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../url/url.gyp:url_lib',
        'google_core_browser',
        'search_engines',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'search/search.cc',
        'search/search.h',
        'search/search_switches.cc',
        'search/search_switches.h',
      ],
    },
  ],
}
