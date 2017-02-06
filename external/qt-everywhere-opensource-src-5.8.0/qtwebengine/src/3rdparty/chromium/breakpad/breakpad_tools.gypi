# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS=="android"', {
      'toolsets': ['host'],
    }],
  ],
  'variables': {
    'clang_warning_flags': ['-Wno-tautological-constant-out-of-range-compare'],
  },
  'include_dirs': [
    'src',
    'src/third_party',
  ],
}
