# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'defines': [
      'JSON_USE_EXCEPTION=0',
    ],
    'sources': [
      'source/include/json/assertions.h',
      'source/include/json/autolink.h',
      'source/include/json/config.h',
      'source/include/json/features.h',
      'source/include/json/forwards.h',
      'source/include/json/json.h',
      'source/include/json/reader.h',
      'overrides/include/json/value.h',
      'source/include/json/writer.h',
      'source/src/lib_json/json_batchallocator.h',
      'overrides/src/lib_json/json_reader.cpp',
      'source/src/lib_json/json_tool.h',
      'overrides/src/lib_json/json_value.cpp',
      'source/src/lib_json/json_writer.cpp',
    ],
    'include_dirs': [
      '../..',
      'overrides/include/',
      'source/include/',
      'source/src/lib_json/',
    ],
    'direct_dependent_settings': {
      'include_dirs': [
        'overrides/include/',
        'source/include/',
      ],
    },
  },
}
