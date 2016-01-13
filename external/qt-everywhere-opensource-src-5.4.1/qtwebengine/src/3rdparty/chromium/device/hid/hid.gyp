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
      'sources': [
        'device_monitor_linux.cc',
        'device_monitor_linux.h',
        'hid_connection.cc',
        'hid_connection.h',
        'hid_connection_linux.cc',
        'hid_connection_linux.h',
        'hid_connection_mac.cc',
        'hid_connection_mac.h',
        'hid_connection_win.cc',
        'hid_connection_win.h',
        'hid_device_info.cc',
        'hid_device_info.h',
        'hid_report_descriptor.cc',
        'hid_report_descriptor.h',
        'hid_report_descriptor_item.cc',
        'hid_report_descriptor_item.h',
        'hid_service.cc',
        'hid_service.h',
        'hid_service_linux.cc',
        'hid_service_linux.h',
        'hid_service_mac.cc',
        'hid_service_mac.h',
        'hid_service_win.cc',
        'hid_service_win.h',
        'hid_usage_and_page.cc',
        'hid_usage_and_page.h',
        'hid_utils_mac.cc',
        'hid_utils_mac.h',
        'input_service_linux.cc',
        'input_service_linux.h',
      ],
      'conditions': [
        ['use_udev==1', {
          'dependencies': [
            '../udev_linux/udev.gyp:udev_linux',
          ],
        }, {  # use_udev==0
          # The Linux implementation is based on Udev.
          'sources!': [
            'device_monitor_linux.cc',
            'device_monitor_linux.h',
            'hid_service_linux.cc',
            'hid_service_linux.h',
            'input_service_linux.cc',
            'input_service_linux.h',
          ],
        }],
      ],
    },
  ],
}
