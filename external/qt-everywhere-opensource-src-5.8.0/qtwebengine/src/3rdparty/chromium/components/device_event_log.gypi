# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'device_event_log_component',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'DEVICE_EVENT_LOG_IMPLEMENTATION',
      ],
      'sources': [
        'device_event_log/device_event_log.cc',
        'device_event_log/device_event_log.h',
        'device_event_log/device_event_log_impl.cc',
        'device_event_log/device_event_log_impl.h',
      ],
    },
  ],
}
