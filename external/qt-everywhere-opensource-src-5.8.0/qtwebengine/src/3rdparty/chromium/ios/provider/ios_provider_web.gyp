# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
   },
  'targets': [
    {
      # GN version: //ios/public/provider/web
      'target_name': 'ios_provider_web',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../web/ios_web.gyp:ios_web',
      ],
      'sources': [
        '../public/provider/web/web_controller_provider.h',
        '../public/provider/web/web_controller_provider.mm',
        '../public/provider/web/web_ui_ios.h',
        '../public/provider/web/web_ui_ios_controller.cc',
        '../public/provider/web/web_ui_ios_controller.h',
        '../public/provider/web/web_ui_ios_controller_factory.h',
        '../public/provider/web/web_ui_ios_message_handler.cc',
        '../public/provider/web/web_ui_ios_message_handler.h',
      ],
    },
  ],
}
