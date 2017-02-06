# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/network_time
      'target_name': 'network_time',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        'client_update_protocol'
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'network_time/network_time_pref_names.cc',
        'network_time/network_time_pref_names.h',
        'network_time/network_time_tracker.cc',
        'network_time/network_time_tracker.h',
      ],
    },
  ],
}
