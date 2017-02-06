# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'search_provider_logos',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../ui/gfx/gfx.gyp:gfx',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'search_provider_logos/google_logo_api.cc',
        'search_provider_logos/google_logo_api.h',
        'search_provider_logos/logo_cache.cc',
        'search_provider_logos/logo_cache.h',
        'search_provider_logos/logo_common.cc',
        'search_provider_logos/logo_common.h',
        'search_provider_logos/logo_tracker.cc',
        'search_provider_logos/logo_tracker.h',
      ],
    },
  ],
}
