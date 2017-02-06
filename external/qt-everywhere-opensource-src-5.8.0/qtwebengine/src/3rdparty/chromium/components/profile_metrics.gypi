# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/profile_metrics
      'target_name': 'profile_metrics',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'profile_metrics/counts.cc',
        'profile_metrics/counts.h',
      ],
    },
  ],
}
