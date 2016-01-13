# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'conditions': [
    [ 'OS=="win"', {
      'includes': [
        'win/sandbox_win.gypi',
      ],
    }],
    [ 'OS=="linux" or OS=="android"', {
      'includes': [
        'linux/sandbox_linux.gypi',
      ],
    }],
    [ 'OS=="mac" and OS!="ios"', {
      'includes': [
        'mac/sandbox_mac.gypi',
      ],
    }],
    [ 'OS!="win" and OS!="mac" and OS!="linux" and OS!="android"', {
      # A 'default' to accomodate the "sandbox" target.
      'targets': [
        {
          'target_name': 'sandbox',
          'type': 'none',
        }
       ]
    }],
  ],
}
