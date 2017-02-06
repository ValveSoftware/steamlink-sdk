# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'webusb',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../device/core/core.gyp:device_core',
        '../device/usb/usb.gyp:device_usb',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'webusb/webusb_browser_client.h',
        'webusb/webusb_detector.cc',
        'webusb/webusb_detector.h',
      ],
    },
  ],
}
