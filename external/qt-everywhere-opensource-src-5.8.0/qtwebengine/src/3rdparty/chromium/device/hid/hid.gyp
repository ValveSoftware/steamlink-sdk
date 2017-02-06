# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'device_hid',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'dependencies': [
        '../../components/components.gyp:device_event_log_component',
        '../../net/net.gyp:net',
        '../core/core.gyp:device_core',
      ],
      'sources': [
        'hid_collection_info.cc',
        'hid_collection_info.h',
        'hid_connection.cc',
        'hid_connection.h',
        'hid_connection_linux.cc',
        'hid_connection_linux.h',
        'hid_connection_mac.cc',
        'hid_connection_mac.h',
        'hid_connection_win.cc',
        'hid_connection_win.h',
        'hid_device_filter.cc',
        'hid_device_filter.h',
        'hid_device_info.cc',
        'hid_device_info.h',
        'hid_device_info_linux.cc',
        'hid_device_info_linux.h',
        'hid_report_descriptor.cc',
        'hid_report_descriptor.h',
        'hid_report_descriptor_item.cc',
        'hid_report_descriptor_item.h',
        'hid_service.cc',
        'hid_service.h',
        'hid_service_mac.cc',
        'hid_service_mac.h',
        'hid_service_win.cc',
        'hid_service_win.h',
        'hid_usage_and_page.cc',
        'hid_usage_and_page.h',
      ],
      'conditions': [
        ['OS=="linux" and use_udev==1', {
          'dependencies': [
            '../udev_linux/udev.gyp:udev_linux',
          ],
          'sources': [
            'fake_input_service_linux.cc',
            'fake_input_service_linux.h',
            'hid_service_linux.cc',
            'hid_service_linux.h',
            'input_service_linux.cc',
            'input_service_linux.h',
          ],
        }],
        ['OS=="win"', {
          'all_dependent_settings': {
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalDependencies': [
                  'hid.lib',
                  'setupapi.lib',
                ],
              },
            },
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalDependencies': [
                'hid.lib',
                'setupapi.lib',
              ],
            },
          },
        }],
      ],
    },
    {
      'target_name': 'device_hid_mocks',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'dependencies': [
        '../../testing/gmock.gyp:gmock',
        'device_hid',
      ],
      'sources': [
        'mock_hid_service.cc',
        'mock_hid_service.h',
      ],
    },
  ],
}
