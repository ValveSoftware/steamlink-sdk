# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/favicon_base
      'target_name': 'favicon_base',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../ui/base/ui_base.gyp:ui_base',
        '../ui/base/ui_base.gyp:ui_data_pack',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../url/url.gyp:url_lib',
      ],
      'sources': [
        'favicon_base/fallback_icon_style.cc',
        'favicon_base/fallback_icon_style.h',
        'favicon_base/fallback_icon_url_parser.cc',
        'favicon_base/fallback_icon_url_parser.h',
        'favicon_base/favicon_callback.h',
        'favicon_base/favicon_types.cc',
        'favicon_base/favicon_types.h',
        'favicon_base/favicon_url_parser.cc',
        'favicon_base/favicon_url_parser.h',
        'favicon_base/favicon_usage_data.cc',
        'favicon_base/favicon_usage_data.h',
        'favicon_base/favicon_util.cc',
        'favicon_base/favicon_util.h',
        'favicon_base/large_icon_url_parser.cc',
        'favicon_base/large_icon_url_parser.h',
        'favicon_base/select_favicon_frames.cc',
        'favicon_base/select_favicon_frames.h',
      ],
    },
  ],
}
