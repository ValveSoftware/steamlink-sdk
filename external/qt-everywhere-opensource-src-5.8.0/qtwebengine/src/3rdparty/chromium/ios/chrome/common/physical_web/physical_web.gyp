# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets':[
    {
      # GN version: //ios/chrome/common/physical_web
      'target_name': 'physical_web',
      'type': 'static_library',
      'sources': [
        'physical_web_device.h',
        'physical_web_device.mm',
        'physical_web_request.h',
        'physical_web_request.mm',
        'physical_web_scanner.h',
        'physical_web_scanner.mm',
        'physical_web_types.h',
        'physical_web_types.mm',
      ],
      'dependencies': [
        '../../../../base/base.gyp:base',
        '../../../../components/components.gyp:version_info',
        '../../../../device/bluetooth/bluetooth.gyp:uribeacon',
        '../../../../google_apis/google_apis.gyp:google_apis',
        '../../../../ios/web/ios_web.gyp:user_agent',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/CoreBluetooth.framework',
          '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
        ],
      },
    }
  ]
}
