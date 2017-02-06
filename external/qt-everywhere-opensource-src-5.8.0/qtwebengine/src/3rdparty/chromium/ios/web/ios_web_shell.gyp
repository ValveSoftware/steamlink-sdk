# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
   },
  'targets': [
    {
      # GN version: //ios/web/shell:ios_web_shell
      'target_name': 'ios_web_shell',
      'includes': [
        'ios_web_shell_exe.gypi',
      ]
    },
  ],
}
