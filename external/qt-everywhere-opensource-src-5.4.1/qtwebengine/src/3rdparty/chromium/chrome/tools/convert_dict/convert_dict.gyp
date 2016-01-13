# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'convert_dict_lib',
      'product_name': 'convert_dict',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'include_dirs': [
        '../../../',
      ],
      'dependencies': [
        '../../../base/base.gyp:base',
      ],
      'sources': [
        'aff_reader.cc',
        'aff_reader.h',
        'dic_reader.cc',
        'dic_reader.h',
        'hunspell_reader.cc',
        'hunspell_reader.h',
      ],
    },
    {
      'target_name': 'convert_dict',
      'type': 'executable',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../base/base.gyp:base_i18n',
        '../../../third_party/hunspell/hunspell.gyp:hunspell',
        'convert_dict_lib',
      ],
      'sources': [
        'convert_dict.cc',
      ],
    },
  ],
}
