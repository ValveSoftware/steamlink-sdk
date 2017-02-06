# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'woff2_dec',
      'type': 'static_library',
      'include_dirs': [
        'src',
        '<(DEPTH)/third_party/brotli/dec',
      ],
      'dependencies': [
        '<(DEPTH)/third_party/brotli/brotli.gyp:brotli',
      ],
      'sources': [
        'src/buffer.h',
        'src/round.h',
        'src/store_bytes.h',
        'src/table_tags.cc',
        'src/table_tags.h',
        'src/variable_length.cc',
        'src/variable_length.h',
        'src/woff2_common.cc',
        'src/woff2_common.h',
        'src/woff2_dec.cc',
        'src/woff2_dec.h',
        'src/woff2_out.cc',
        'src/woff2_out.h',
      ],
      # TODO(ksakamoto): http://crbug.com/167187
      'msvs_disabled_warnings': [
        4267,
      ],
    },
  ],
}
