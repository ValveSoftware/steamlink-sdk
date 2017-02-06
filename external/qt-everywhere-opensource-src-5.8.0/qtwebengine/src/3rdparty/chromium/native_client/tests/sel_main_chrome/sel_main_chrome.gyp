# -*- gyp -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
     {
      # This tests linking of sel_main_chrome.c in the Gyp build.  We
      # do not run this Gyp-built executable.
      'target_name': 'sel_main_chrome_test',
      'type': 'executable',
      'dependencies': [
        '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:sel_main_chrome',
      ],
      'sources': [
        'sel_main_chrome_test.cc',
      ],
    },
  ],
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          # This tests linking of sel_main_chrome.c in the Gyp build.  We
          # do not run this Gyp-built executable.
          'target_name': 'sel_main_chrome_test64',
          'type': 'executable',
          'variables': {
            'win_target': 'x64',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:sel_main_chrome64',
          ],
          'sources': [
            'sel_main_chrome_test.cc',
          ],
        },
      ],
    }],
  ],
}
