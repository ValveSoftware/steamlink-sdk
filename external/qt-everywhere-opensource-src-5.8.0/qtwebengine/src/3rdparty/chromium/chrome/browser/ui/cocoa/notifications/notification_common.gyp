# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
'targets': [
  {
    # GN version: //chrome/browser/ui/notifications:common
    'target_name': 'notification_common',
    'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)',
      ],
    'sources': [
      'notification_builder_mac.h',
      'notification_builder_mac.mm',
      'notification_constants_mac.h',
      'notification_constants_mac.mm',
      'notification_response_builder_mac.h',
      'notification_response_builder_mac.mm',
    ],
    'dependencies': [
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/chrome/chrome_resources.gyp:chrome_strings',
      '<(DEPTH)/ui/base/ui_base.gyp:ui_base',
    ],
  },
],
}
