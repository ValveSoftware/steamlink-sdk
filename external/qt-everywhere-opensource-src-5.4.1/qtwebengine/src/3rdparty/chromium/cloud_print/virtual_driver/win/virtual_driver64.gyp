# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'virtual_driver_suffix': '64',
  },
  'target_defaults': {
    'defines': [
      '<@(nacl_win64_defines)',
    ],
    'dependencies': [
      '<(DEPTH)/base/base.gyp:base_win64',
      '<(DEPTH)/chrome/chrome.gyp:launcher_support64',
      '<(DEPTH)/chrome/common_constants.gyp:common_constants_win64',
    ],
    'configurations': {
      'Common_Base': {
        'msvs_target_platform': 'x64',
      },
    },
  },
  'includes': [
    'virtual_driver.gypi',
  ],
}

