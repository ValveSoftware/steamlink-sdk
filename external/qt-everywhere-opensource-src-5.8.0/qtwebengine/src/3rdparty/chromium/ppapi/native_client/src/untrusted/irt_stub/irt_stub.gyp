# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../../build/common_untrusted.gypi',
  ],
  'targets' : [
    {
      'target_name': 'ppapi_stub_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libppapi_stub.a',
        'build_glibc': 1,
        'build_newlib': 1,
        'build_pnacl_newlib': 1,
      },
      # Always compile libppapi_stub with -fPIC so that -lppapi can be linked
      # into shared libraries (libppapi.so is a linker script that pulls in
      # ppapi_stub).
      'compile_flags': [ '-fPIC' ],
      'include_dirs': [
        '../../../..',
      ],
      'sources': [
        'plugin_main_irt.c',
        'ppapi_plugin_main.c',
        'ppapi_plugin_start.c',
        'thread_creator.c'
      ],
    },
  ],
}
