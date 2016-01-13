# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'lzma_sdk_sources': [
      '7z.h',
      '7zAlloc.c',
      '7zAlloc.h',
      '7zBuf.c',
      '7zBuf.h',
      '7zCrc.c',
      '7zCrc.h',
      '7zCrcOpt.c',
      '7zDec.c',
      '7zFile.c',
      '7zFile.h',
      '7zIn.c',
      '7zStream.c',
      'Alloc.c',
      'Alloc.h',
      'Bcj2.c',
      'Bcj2.h',
      'Bra.c',
      'Bra.h',
      'Bra86.c',
      'CpuArch.c',
      'CpuArch.h',
      'LzFind.c',
      'LzFind.h',
      'LzHash.h',
      'Lzma2Dec.c',
      'Lzma2Dec.h',
      'LzmaEnc.c',
      'LzmaEnc.h',
      'LzmaDec.c',
      'LzmaDec.h',
      'LzmaLib.c',
      'LzmaLib.h',
      'Types.h',
    ],
  },
  'targets': [
    {
      'target_name': 'lzma_sdk',
      'type': 'static_library',
      'defines': [
        '_7ZIP_ST',
        '_LZMA_PROB32',
      ],
      'sources': [
        '<@(lzma_sdk_sources)',
      ],
      'include_dirs': [
        '.',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'lzma_sdk64',
          'type': 'static_library',
          'defines': [
            '_7ZIP_ST',
            '_LZMA_PROB32',
          ],
          'include_dirs': [
            '.',
          ],
          'sources': [
            '<@(lzma_sdk_sources)',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
          'direct_dependent_settings': {
            'include_dirs': [
              '.',
            ],
          },
        },
      ],
    }],
  ],
}
