# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'constrained_window',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/components/components.gyp:web_modal',
        '<(DEPTH)/ui/views/views.gyp:views',
        '<(DEPTH)/skia/skia.gyp:skia',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'constrained_window/constrained_window_views.cc',
        'constrained_window/constrained_window_views.h',
        'constrained_window/constrained_window_views_client.h',
      ],
    },
  ],
}
