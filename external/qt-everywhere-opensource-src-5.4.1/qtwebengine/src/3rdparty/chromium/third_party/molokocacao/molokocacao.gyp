# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'molokocacao',
      'type': 'static_library',
      'dependencies': [
        '../google_toolbox_for_mac/google_toolbox_for_mac.gyp:google_toolbox_for_mac',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
        ],
      },
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'NSBezierPath+MCAdditions.h',
        'NSBezierPath+MCAdditions.m',
      ],
    },
  ],
}
