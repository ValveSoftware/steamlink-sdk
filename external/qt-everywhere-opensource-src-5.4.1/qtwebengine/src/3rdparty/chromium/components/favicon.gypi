# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'favicon_core',
      'type': 'static_library',
      'dependencies': [
        '../ui/gfx/gfx.gyp:gfx_geometry',
        'favicon_base',
      ],
      'sources': [
        'favicon/core/browser/favicon_client.h',
        'favicon/core/favicon_driver.h',
        'favicon/core/favicon_url.cc',
        'favicon/core/favicon_url.h',
      ],
      'include_dirs': [
        '..',
      ],
    },
  ],
}
