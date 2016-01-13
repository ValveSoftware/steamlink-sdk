# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Build the rezip build tool.
{
  'targets': [
    {
      'target_name': 'rezip',
      'type': 'executable',
      'toolsets': [ 'host' ],
      'dependencies': [
        '<(DEPTH)/third_party/zlib/zlib.gyp:minizip',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'sources': [
        'rezip/rezip.cc',
      ],
    }
  ],
}
