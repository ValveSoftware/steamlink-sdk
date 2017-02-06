# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


{
  'includes': [
    '../../../build/common.gypi',
  ],
  'targets' : [
    {
      'target_name': 'nosys_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libnosys.a',
        'build_glibc': 0,
        'build_newlib': 1,
        'build_pnacl_newlib': 1,
      },
      'sources': [],
    },
  ],
}
