# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'devtools_discovery',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        '../content/content.gyp:content_common',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'devtools_discovery/basic_target_descriptor.cc',
        'devtools_discovery/basic_target_descriptor.h',
        'devtools_discovery/devtools_discovery_manager.cc',
        'devtools_discovery/devtools_discovery_manager.h',
        'devtools_discovery/devtools_target_descriptor.h',
      ],
    },
  ],
}
