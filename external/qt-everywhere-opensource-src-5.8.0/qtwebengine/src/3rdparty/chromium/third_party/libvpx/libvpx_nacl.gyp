# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'libvpx_source%': 'source/libvpx',
    'werror': '',
  },
  'includes': [
    '../../native_client/build/untrusted.gypi',
  ],
  'targets': [
    {
      'target_name': 'libvpx_nacl',
      'type': 'none',
      'variables': {
        'nlib_target': 'libvpx_nacl.a',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_pnacl_newlib': 1,
      },
      'include_dirs': [
        'source/config/nacl',
        'source/config',
        '<(libvpx_source)',
        '<(libvpx_source)/vp8/common',
        '<(libvpx_source)/vp8/decoder',
        '<(libvpx_source)/vp8/encoder',
      ],
      'includes': ['libvpx_srcs_nacl.gypi', ],
    },  # end of target 'libvpx_nacl'
  ],
}
