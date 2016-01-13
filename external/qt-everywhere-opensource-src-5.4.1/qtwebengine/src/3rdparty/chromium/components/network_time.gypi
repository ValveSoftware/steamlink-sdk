# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'network_time',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'network_time/network_time_tracker.cc',
        'network_time/network_time_tracker.h',
        'network_time/network_time_pref_names.cc',
        'network_time/network_time_pref_names.h',
      ],
    },
  ],
}
