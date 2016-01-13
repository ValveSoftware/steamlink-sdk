# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS=="linux"', {
      'sources': [
        'posix/get_plugin_dirname.cc',
        'posix/sel_ldr_launcher_posix.cc',
      ],
    }],
    ['OS=="mac"', {
      'sources': [
        'osx/get_plugin_dirname.mm',
        'posix/sel_ldr_launcher_posix.cc',
      ],
      'xcode_settings': {
        'WARNING_CFLAGS!': [
          '-pedantic',  # import is a gcc extension
        ],
      },
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
        ],
      },
    }],
    ['OS=="win"', {
      'sources': [
        'win/sel_ldr_launcher_win.cc',
      ],
    }],
  ],
}
