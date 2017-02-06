# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'tls_edit',
      'type': 'executable',
      'toolsets': ['host'],
      'sources': [
        'tls_edit.c',
      ],
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/trusted/validator_ragel/rdfa_validator.gyp:rdfa_validator',
      ],
    },
  ],
}
