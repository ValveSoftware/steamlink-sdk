# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'user_prefs',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_prefs',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../content/content.gyp:content_browser',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'USER_PREFS_IMPLEMENTATION',
      ],
      'sources': [
        'user_prefs/user_prefs.cc',
        'user_prefs/user_prefs.h',
        'user_prefs/user_prefs_export.h',
      ],
    },
  ],
}
