# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'libwebm',
      'type': 'static_library',
      'conditions': [
        ['OS!="win"', {
          'cflags': [
            '-Wno-deprecated-declarations',  # libwebm uses std::auto_ptr
          ],
        }],
      ],
      'include_dirs': [
        './source',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          './source',
        ],
      },
      'sources': [
        'source/mkvmuxer/mkvmuxer.cc',
        'source/mkvmuxer/mkvmuxerutil.cc',
        'source/mkvmuxer/mkvwriter.cc',
      ],
    },  # target libwebm
  ]
}
