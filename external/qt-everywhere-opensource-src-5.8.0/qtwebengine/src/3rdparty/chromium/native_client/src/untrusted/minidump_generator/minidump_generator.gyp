# -*- gyp -*-
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'targets' : [
    {
      'target_name': 'minidump_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libminidump_generator.a',
        'nso_target': 'libminidump_generator.so',
        'build_glibc': 1,
        'build_newlib': 1,
        'build_pnacl_newlib': 1,
        # Allow Breakpad headers to #include other Breakpad headers.
        'include_dirs': ['<(DEPTH)/breakpad/src'],
      },
      'sources': [
        'build_id.cc',
        'minidump_generator.cc',
      ],
    },
  ],
}
