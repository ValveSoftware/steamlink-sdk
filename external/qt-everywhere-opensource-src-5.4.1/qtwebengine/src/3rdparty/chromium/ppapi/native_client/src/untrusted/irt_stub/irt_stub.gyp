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
      'include_dirs': [
        '../../../..',
      ],
      'sources': [
        'ppapi_plugin_main.c',
        'ppapi_plugin_start.c',
        'plugin_main_irt.c',
        'thread_creator.c'
      ],
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
  ],
}
