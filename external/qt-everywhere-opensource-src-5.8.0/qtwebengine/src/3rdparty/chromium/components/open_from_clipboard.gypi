# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/open_from_clipboard
      'target_name': 'open_from_clipboard',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'open_from_clipboard/clipboard_recent_content.cc',
        'open_from_clipboard/clipboard_recent_content.h',
        'open_from_clipboard/clipboard_recent_content_ios.h',
        'open_from_clipboard/clipboard_recent_content_ios.mm',
      ],
    },
    {
      # GN version: //components/open_from_clipboard:test_support
      'target_name': 'open_from_clipboard_test_support',
      'type': 'static_library',
      'dependencies': [
        'open_from_clipboard',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'open_from_clipboard/fake_clipboard_recent_content.cc',
        'open_from_clipboard/fake_clipboard_recent_content.h',
      ],
    },
  ],
}
