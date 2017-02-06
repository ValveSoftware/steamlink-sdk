# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ui/content_accelerators
      'target_name': 'ui_content_accelerators',
      'type': 'static_library',
      'dependencies': [
        '../base/ui_base.gyp:ui_base',
        '../events/events.gyp:events',
        '../../content/content.gyp:content_browser',
        '../../third_party/WebKit/public/blink_headers.gyp:blink_headers',
      ],
      'sources' : [
        # Note: file list duplicated in GN build.
        'accelerator_util.cc',
        'accelerator_util.h',
      ],
    },
  ],
}
