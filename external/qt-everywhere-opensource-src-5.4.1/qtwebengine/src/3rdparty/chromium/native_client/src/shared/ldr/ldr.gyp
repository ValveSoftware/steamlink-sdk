# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'ldr',
      'type': 'static_library',
      'sources': [
        'nacl_ldr.c',
        'nacl_ldr.h',
      ],
    }
  ]
}
