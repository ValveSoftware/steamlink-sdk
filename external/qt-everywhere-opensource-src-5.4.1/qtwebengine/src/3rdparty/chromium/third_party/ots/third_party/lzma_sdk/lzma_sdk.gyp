# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# lzma_sdk for standalone build.
{
  'targets': [
    {
      'target_name': 'ots_lzma_sdk',
      'type': 'static_library',
      'defines': [
        '_7ZIP_ST', # Disable multi-thread support.
        '_LZMA_PROB32', # This could increase the speed on 32bit platform.
      ],
      'sources': [
        'Alloc.c',
        'Alloc.h',
        'LzFind.c',
        'LzFind.h',
        'LzHash.h',
        'LzmaEnc.c',
        'LzmaEnc.h',
        'LzmaDec.c',
        'LzmaDec.h',
        'LzmaLib.c',
        'LzmaLib.h',
        'Types.h',
      ],
      'include_dirs': [
        '.',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../..',
        ],
      },
    },
  ],
}
