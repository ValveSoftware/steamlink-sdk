# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [],
  'conditions': [
    # Our shared library hack only works with ninja; xcode cannot generate
    # iOS build targets for dynamic libraries.
    ['"<(GENERATOR)"=="ninja"', {
      'targets': [
        {
          'target_name': 'crnet_consumer',
          'type': 'executable',
          'mac_bundle': 1,
          'dependencies': [
            '../../../ios/crnet/crnet_pack.gyp:crnet_pack',
          ],
          'sources': [
            'crnet_consumer_app_delegate.h',
            'crnet_consumer_app_delegate.mm',
            'crnet_consumer_view_controller.h',
            'crnet_consumer_view_controller.m',
            'main.mm',
          ],
          'mac_bundle_resources': [
            '../../../third_party/icu/common/icudtl.dat',
            'Default.png',
            'crnet-consumer-Info.plist',
          ],
          'include_dirs': [
            '../../..',
            '..',
          ],
          'link_settings': {
            'libraries': [
              '<(PRODUCT_DIR)/libcrnet_standalone.a',
              '$(SDKROOT)/System/Library/Frameworks/CFNetwork.framework',
              '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
              '$(SDKROOT)/System/Library/Frameworks/CoreGraphics.framework',
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
              '$(SDKROOT)/System/Library/Frameworks/MobileCoreServices.framework',
              '$(SDKROOT)/System/Library/Frameworks/UIKit.framework',
              '$(SDKROOT)/System/Library/Frameworks/Security.framework',
              '$(SDKROOT)/System/Library/Frameworks/SystemConfiguration.framework',
            ],
            'xcode_settings': {
              'OTHER_LDFLAGS': [
                '-lresolv',
                '-lsqlite3',
                '-lxml2',
              ],
            },
          },
          'xcode_settings': {
            'CLANG_ENABLE_OBJC_ARC': 'YES',
            'DEAD_CODE_STRIPPING': 'YES',
            'INFOPLIST_FILE': 'crnet-consumer-Info.plist',
          },
        },
      ],
    }],
  ],
}
