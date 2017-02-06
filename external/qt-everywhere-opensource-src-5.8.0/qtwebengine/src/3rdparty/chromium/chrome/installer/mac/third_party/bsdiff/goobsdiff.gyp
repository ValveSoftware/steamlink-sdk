# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'include_dirs': [
      '../../../../..',
    ],
    'link_settings': {
      'libraries': [
        '$(SDKROOT)/usr/lib/libbz2.dylib',
        '$(SDKROOT)/usr/lib/libz.dylib',
      ],
    },
    'configurations': {
      'Release': {
        'xcode_settings': {
          # Use -Os to minimize the size of the installer tools.
          'GCC_OPTIMIZATION_LEVEL': 's',
        },
      },
    },
  },
  'targets': [
    {
      # Because size is a concern, don't link against all of base. Instead,
      # just bring in a copy of the one component that's needed, along with
      # the adapter that allows it to be called from C (not C++) code.
      'target_name': 'goobsdiff_sha1_adapter',
      'type': 'static_library',
      'sources': [
        '../../../../../base/sha1.cc',
        'sha1_adapter.cc',
        'sha1_adapter.h',
      ],
    },
    {
      'target_name': 'goobsdiff',
      'type': 'executable',
      'dependencies': [
        'goobsdiff_sha1_adapter',
        '../xz/xz.gyp:lzma',
      ],
      'sources': [
        'empty.cc',
        'goobsdiff.c',
      ],
    },
    {
      'target_name': 'goobspatch',
      'type': 'executable',
      'dependencies': [
        'goobsdiff_sha1_adapter',
        '../xz/xz.gyp:lzma_decompress',
      ],
      'sources': [
        'empty.cc',
        'goobspatch.c',
      ],
    },
  ],
}
