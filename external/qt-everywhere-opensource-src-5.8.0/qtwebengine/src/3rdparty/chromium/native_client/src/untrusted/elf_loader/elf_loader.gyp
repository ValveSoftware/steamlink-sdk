# -*- gyp -*-
# Copyright (c) 2015 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'targets' : [
    {
      'target_name': 'elf_loader_nexe',
      'type': 'none',
      'variables': {
        'nexe_target': 'elf_loader',
        'build_glibc': 0,
        'build_newlib': 1,
        'build_pnacl_newlib': 0,
      },
      'sources': [
        'elf_loader.c',
      ],
      'dependencies': [
        '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:nacl_lib_newlib',
      ],
    },
  ],
}
