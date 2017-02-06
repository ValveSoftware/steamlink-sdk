# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //device/power_save_blocker
      'target_name': 'device_power_save_blocker',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
      ],
      'defines': [
        'DEVICE_POWER_SAVE_BLOCKER_IMPLEMENTATION',
      ],
      'sources': [
        # Note: file list duplicated in GN build, other than Android, as gyp
        # for Android isn't supported.
        'power_save_blocker.h',
        'power_save_blocker_chromeos.cc',
        'power_save_blocker_mac.cc',
        'power_save_blocker_ozone.cc',
        'power_save_blocker_win.cc',
        'power_save_blocker_x11.cc',
      ],
      'conditions': [
        ['use_x11==1 and chromeos==0', {
          'dependencies': [
            '../../build/linux/system.gyp:xext',
            '../../build/linux/system.gyp:xscrnsaver',
            '../../ui/gfx/x/gfx_x11.gyp:gfx_x11',
          ],
        }],
        ['use_dbus==1', {
          'dependencies': [
            '../../build/linux/system.gyp:dbus',
            '../../dbus/dbus.gyp:dbus',
          ],
        }],
        ['chromeos==1', {
          'sources!': [
            'power_save_blocker_ozone.cc',
            'power_save_blocker_x11.cc',
          ],
          'dependencies': [
            '../../chromeos/chromeos.gyp:chromeos',
            '../../chromeos/chromeos.gyp:power_manager_proto',
          ],
        }],
        ['OS == "mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
              '$(SDKROOT)/System/Library/Frameworks/IOKit.framework',
            ],
          },
        }],
      ],
    },
  ],
}
