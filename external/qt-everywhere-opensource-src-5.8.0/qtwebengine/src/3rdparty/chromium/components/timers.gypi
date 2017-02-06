# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/timers
      'target_name': 'timers',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'timers/alarm_timer_chromeos.cc',
        'timers/alarm_timer_chromeos.h',
      ],
    },
  ],
}
