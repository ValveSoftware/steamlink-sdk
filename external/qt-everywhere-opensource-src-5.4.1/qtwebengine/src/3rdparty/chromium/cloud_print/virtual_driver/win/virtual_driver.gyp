# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'virtual_driver_suffix': '',
  },
  'target_defaults': {
    'dependencies': [
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/chrome/chrome.gyp:launcher_support',
      '<(DEPTH)/chrome/common_constants.gyp:common_constants',
    ],

  },
  'includes': [
    'virtual_driver.gypi',
  ],
}
