# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/bubble
      'target_name': 'bubble',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'bubble/bubble_close_reason.h',
        'bubble/bubble_controller.cc',
        'bubble/bubble_controller.h',
        'bubble/bubble_delegate.cc',
        'bubble/bubble_delegate.h',
        'bubble/bubble_manager.cc',
        'bubble/bubble_manager.h',
        'bubble/bubble_reference.h',
        'bubble/bubble_ui.h',
      ],
    },
    {
      'target_name': 'bubble_test_support',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../testing/gmock.gyp:gmock',
        'bubble',
      ],
      'sources': [
        'bubble/bubble_manager_mocks.cc',
        'bubble/bubble_manager_mocks.h',
      ],
    },
  ],
}
