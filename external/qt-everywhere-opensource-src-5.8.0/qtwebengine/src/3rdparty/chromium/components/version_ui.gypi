# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/version_ui
      'target_name': 'version_ui',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'variations',
        '../base/base.gyp:base',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'version_ui/version_handler_helper.cc',
        'version_ui/version_handler_helper.h',
        'version_ui/version_ui_constants.cc',
        'version_ui/version_ui_constants.h',
      ],
    },
  ],
}
