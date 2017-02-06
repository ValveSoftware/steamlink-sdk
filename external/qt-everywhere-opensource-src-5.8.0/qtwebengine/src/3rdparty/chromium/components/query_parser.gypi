# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'query_parser',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../third_party/icu/icu.gyp:icuuc',
      ],
      'sources': [
        'query_parser/query_parser.cc',
        'query_parser/query_parser.h',
        'query_parser/snippet.cc',
        'query_parser/snippet.h',
      ],
    },
  ],
}
