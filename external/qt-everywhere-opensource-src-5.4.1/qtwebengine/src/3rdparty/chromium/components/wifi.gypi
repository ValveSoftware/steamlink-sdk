# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'wifi_component',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/libxml/libxml.gyp:libxml',
        'onc_component',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'WIFI_IMPLEMENTATION',
      ],
      'sources': [
        'wifi/network_properties.cc',
        'wifi/network_properties.h',
        'wifi/wifi_export.h',
        'wifi/wifi_service.cc',
        'wifi/wifi_service.h',
        'wifi/wifi_service_mac.mm',
        'wifi/wifi_service_win.cc',
      ],
      'conditions': [
        ['OS == "win"', {
          'link_settings': {
            'libraries': [
              '-liphlpapi.lib',
            ],
          },
        }],
        ['OS == "mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/CoreWLAN.framework',
              '$(SDKROOT)/System/Library/Frameworks/SystemConfiguration.framework',
            ]
          },
        }],
      ],
    },
    {
      'target_name': 'wifi_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        'onc_component',
        'wifi_component',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'wifi/fake_wifi_service.cc',
        'wifi/fake_wifi_service.h',
      ],
    },
    {
      'target_name': 'wifi_test',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        'onc_component',
        'wifi_component',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'wifi/wifi_test.cc',
      ],
    },
  ],
}
