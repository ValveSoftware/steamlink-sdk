# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# TODO(bashi): Rename this directory to font-compression-reference

{
  'targets': [
    {
      'target_name': 'brotli',
      'type': 'static_library',
      'include_dirs': [
        'src/brotli/dec',
      ],
      'sources': [
        'src/brotli/dec/bit_reader.c',
        'src/brotli/dec/bit_reader.h',
        'src/brotli/dec/context.h',
        'src/brotli/dec/decode.c',
        'src/brotli/dec/decode.h',
        'src/brotli/dec/dictionary.h',
        'src/brotli/dec/huffman.c',
        'src/brotli/dec/huffman.h',
        'src/brotli/dec/prefix.h',
        'src/brotli/dec/safe_malloc.c',
        'src/brotli/dec/safe_malloc.h',
        'src/brotli/dec/streams.c',
        'src/brotli/dec/streams.h',
        'src/brotli/dec/transform.h',
        'src/brotli/dec/types.h',
      ],
    },
    {
      'target_name': 'woff2_dec',
      'type': 'static_library',
      'include_dirs': [
        'src/brotli/dec',
        'src/woff2',
      ],
      'dependencies': [
        'brotli',
      ],
      'sources': [
        'src/woff2/buffer.h',
        'src/woff2/round.h',
        'src/woff2/store_bytes.h',
        'src/woff2/table_tags.cc',
        'src/woff2/table_tags.h',
        'src/woff2/woff2_common.h',
        'src/woff2/woff2_dec.cc',
        'src/woff2/woff2_dec.h',
      ],
    },
  ],
}
