# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'libwebm.gypi',
    '../../native_client/build/untrusted.gypi',
  ],
  'targets': [
    {
      'target_name': 'libwebm_nacl',
      'type': 'none',
      'variables': {
        'nlib_target': 'libwebm_nacl.a',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_pnacl_newlib': 1,
      },
      'dependencies': [
        '../../native_client/tools.gyp:prep_toolchain',
      ],
      'sources': [
        '<@(libwebm_sources)',
      ],
      'defines': [
        # LLONG_MIN and UULONG_MAX are defined in newlib only for C++11.
        'LLONG_MIN=LONG_LONG_MIN',
        'ULLONG_MAX=ULONG_LONG_MAX',
      ],
    },  # target libwebm_nacl
  ]
}
