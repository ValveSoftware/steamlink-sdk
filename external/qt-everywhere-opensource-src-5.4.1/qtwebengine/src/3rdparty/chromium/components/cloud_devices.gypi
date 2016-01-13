# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'cloud_devices_common',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
      ],
      'sources': [
        'cloud_devices/common/cloud_device_description.cc',
        'cloud_devices/common/cloud_device_description.h',
        'cloud_devices/common/cloud_device_description_consts.cc',
        'cloud_devices/common/cloud_device_description_consts.h',
        'cloud_devices/common/cloud_devices_switches.cc',
        'cloud_devices/common/cloud_devices_switches.h',
        'cloud_devices/common/cloud_devices_urls.cc',
        'cloud_devices/common/cloud_devices_urls.h',
        'cloud_devices/common/description_items.h',
        'cloud_devices/common/description_items_inl.h',
        'cloud_devices/common/printer_description.cc',
        'cloud_devices/common/printer_description.h',
      ],
    },
  ],
}
