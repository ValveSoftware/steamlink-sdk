# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'lzma_sdk_sources': [
      '7z.h',
      '7zAlloc.c',
      '7zAlloc.h',
      '7zArcIn.c',
      '7zBuf.c',
      '7zBuf.h',
      '7zCrc.c',
      '7zCrc.h',
      '7zCrcOpt.c',
      '7zDec.c',
      '7zFile.c',
      '7zFile.h',
      '7zStream.c',
      '7zTypes.h',
      'Alloc.c',
      'Alloc.h',
      'Bcj2.c',
      'Bcj2.h',
      'Bra.c',
      'Bra.h',
      'Bra86.c',
      'Compiler.h',
      'CpuArch.c',
      'CpuArch.h',
      'Delta.c',
      'Delta.h',
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
      'Precomp.h',
    ],
  },
  'targets': [
    {
      'target_name': 'lzma_sdk',
      'type': 'static_library',
      'defines': [
        '_7ZIP_ST',
        '_7Z_NO_METHODS_FILTERS',
        '_LZMA_PROB32',
      ],
      'variables': {
        # Upstream uses self-assignment to avoid warnings.
        'clang_warning_flags': [ '-Wno-self-assign' ]
      },
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
          'variables': {
            # Upstream uses self-assignment to avoid warnings.
            'clang_warning_flags': [ '-Wno-self-assign' ]
          },
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
