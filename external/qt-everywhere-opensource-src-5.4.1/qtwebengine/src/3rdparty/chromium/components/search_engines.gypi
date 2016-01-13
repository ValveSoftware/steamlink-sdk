# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'search_engines',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../url/url.gyp:url_lib',
        'search_engines/prepopulated_engines.gyp:prepopulated_engines',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'search_engines/search_engine_type.h',
        'search_engines/search_terms_data.cc',
        'search_engines/search_terms_data.h',
        'search_engines/template_url_data.cc',
        'search_engines/template_url_data.h',
        'search_engines/template_url_id.h',
      ],
    },
  ],
}
