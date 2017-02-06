# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/onc
      'target_name': 'onc_component',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'ONC_IMPLEMENTATION',
      ],
      'sources': [
        'onc/onc_constants.cc',
        'onc/onc_constants.h',
        'onc/onc_export.h',
      ],
    },
  ],
}
