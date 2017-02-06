# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //device/gamepad
      'target_name': 'device_gamepad',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      ],
      'defines': [
        'DEVICE_GAMEPAD_IMPLEMENTATION',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'gamepad_consumer.cc',
        'gamepad_consumer.h',
        'gamepad_data_fetcher.cc',
        'gamepad_data_fetcher.h',
        'gamepad_platform_data_fetcher.h',
        'gamepad_platform_data_fetcher_android.cc',
        'gamepad_platform_data_fetcher_android.h',
        'gamepad_platform_data_fetcher_linux.cc',
        'gamepad_platform_data_fetcher_linux.h',
        'gamepad_platform_data_fetcher_mac.h',
        'gamepad_platform_data_fetcher_mac.mm',
        'gamepad_platform_data_fetcher_win.cc',
        'gamepad_platform_data_fetcher_win.h',
        'gamepad_provider.cc',
        'gamepad_provider.h',
        'gamepad_standard_mappings.cc',
        'gamepad_standard_mappings.h',
        'gamepad_standard_mappings_linux.cc',
        'gamepad_standard_mappings_mac.mm',
        'gamepad_standard_mappings_win.cc',
        'gamepad_user_gesture.cc',
        'gamepad_user_gesture.h',
        'raw_input_data_fetcher_win.cc',
        'raw_input_data_fetcher_win.h',
        'xbox_data_fetcher_mac.cc',
        'xbox_data_fetcher_mac.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'msvs_disabled_warnings': [4267, ],
        }],
        ['OS=="linux" and use_udev==1', {
          'dependencies': [
            '../udev_linux/udev.gyp:udev_linux',
          ]
        }],
        ['OS!="win" and OS!="mac" and OS!="android" and (OS!="linux" or use_udev==0)', {
          'sources': [
            'gamepad_platform_data_fetcher.cc',
          ],
          'sources!': [
            'gamepad_platform_data_fetcher_linux.cc',
          ],
        }],
      ],
    },
    {
      # GN version: //device/gamepad:test_helpers
      'target_name': 'device_gamepad_test_helpers',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        'device_gamepad',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'gamepad_test_helpers.cc',
        'gamepad_test_helpers.h',
      ],
    },
  ],
}
