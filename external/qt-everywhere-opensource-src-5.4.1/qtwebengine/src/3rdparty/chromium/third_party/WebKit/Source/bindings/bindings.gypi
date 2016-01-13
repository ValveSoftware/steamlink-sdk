# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
    'includes': [
        'v8/v8.gypi',
    ],
    'variables': {
        'bindings_dir': '.',
        'bindings_unittest_files': [
            '<@(bindings_v8_unittest_files)',
        ],
    },
}
