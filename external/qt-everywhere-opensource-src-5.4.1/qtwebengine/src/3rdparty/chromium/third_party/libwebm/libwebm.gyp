# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'libwebm.gypi',
  ],
  'targets': [
    {
      'target_name': 'libwebm',
      'type': 'static_library',
      'sources': [
        '<@(libwebm_sources)'
      ],
      'defines!': [
        # This macro is declared in common.gypi which causes warning when
        # compiling mkvmuxerutil.cpp which also defines it.
        '_CRT_RAND_S',
      ],
      'msvs_disabled_warnings': [ 4267 ],
    },  # target libwebm
  ]
}
