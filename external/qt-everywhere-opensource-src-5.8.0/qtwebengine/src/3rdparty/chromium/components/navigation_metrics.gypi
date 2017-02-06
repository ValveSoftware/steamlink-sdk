# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'navigation_metrics',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../url/url.gyp:url_lib',
        'keyed_service_core',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'navigation_metrics/navigation_metrics.cc',
        'navigation_metrics/navigation_metrics.h',
        'navigation_metrics/origins_seen_service.cc',
        'navigation_metrics/origins_seen_service.h',
      ],
    },
  ],
}
