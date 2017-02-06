# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/chooser_controller
      'target_name': 'chooser_controller',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'chooser_controller/chooser_controller.cc',
        'chooser_controller/chooser_controller.h',
      ],
    },
    {
      'target_name': 'chooser_controller_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gmock.gyp:gmock',
        'chooser_controller',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'chooser_controller/mock_chooser_controller.cc',
        'chooser_controller/mock_chooser_controller.h',
      ],
    },
  ],
}
