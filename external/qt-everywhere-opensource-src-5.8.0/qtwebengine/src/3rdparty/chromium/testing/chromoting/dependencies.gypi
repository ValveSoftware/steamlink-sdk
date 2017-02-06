# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included by chromoting's integration_tests.

{
  'dependencies': [
    '../../chrome/chrome.gyp:browser_tests',
    '../../remoting/remoting.gyp:remoting_webapp',
  ],
  'type': 'none',
  'includes': [
    '../../build/isolate.gypi',
  ],
  'conditions': [
    ['OS=="linux"', {
      'dependencies': [
        '../../remoting/remoting.gyp:chromoting_test_driver',
        '../../remoting/remoting.gyp:remoting_me2me_host_archive',
      ],
    }],  # OS=="linux"
  ],
}
