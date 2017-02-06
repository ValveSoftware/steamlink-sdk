# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Keep in sync with remoting/remoting_enable.gni.

{
  'variables': {
    'enable_remoting_host%': 0,
    'enable_me2me_host%': 0,

    'conditions': [
      ['OS=="win" or OS=="mac"', {
        'enable_remoting_host': 1,
        'enable_me2me_host': 1,
      }],
      ['OS=="linux" and chromeos==0 and use_x11==1', {
        'enable_remoting_host': 1,
        'enable_me2me_host': 1,
      }],
      ['OS=="linux" and chromeos==1', {
        'enable_remoting_host': 1,
      }],
    ],  # end of conditions
  },  # end of variables
}
