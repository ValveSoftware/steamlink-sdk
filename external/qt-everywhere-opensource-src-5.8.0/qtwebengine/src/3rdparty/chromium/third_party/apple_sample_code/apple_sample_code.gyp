# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'apple_sample_code',
      'type': 'static_library',
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
        ],
      },
      'sources': [
        'ImageAndTextCell.h',
        'ImageAndTextCell.m',
      ],
    },
  ],
}
