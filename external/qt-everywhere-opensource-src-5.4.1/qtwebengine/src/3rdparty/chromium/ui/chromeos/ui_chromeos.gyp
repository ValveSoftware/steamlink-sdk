# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ui_chromeos',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../skia/skia.gyp:skia',
        '../aura/aura.gyp:aura',
        '../events/events.gyp:events',
        '../wm/wm.gyp:wm',
      ],
      'defines': [
        'UI_CHROMEOS_IMPLEMENTATION',
      ],
      'sources': [
        'touch_exploration_controller.cc',
        'touch_exploration_controller.h',
        'user_activity_power_manager_notifier.cc',
        'user_activity_power_manager_notifier.h',
      ],
    },
  ],
}
