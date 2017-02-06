# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'json_schema',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/re2/re2.gyp:re2'
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'json_schema/json_schema_constants.cc',
        'json_schema/json_schema_constants.h',
        'json_schema/json_schema_validator.cc',
        'json_schema/json_schema_validator.h',
      ],
    },
  ],
}
