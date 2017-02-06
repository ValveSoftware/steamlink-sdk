# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../native_client/build/untrusted.gypi',
    'ppapi_ipc.gypi',
  ],
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'ppapi_ipc_nacl',
          'type': 'none',
          'variables': {
            'ppapi_ipc_target': 1,
            'nacl_win64_target': 0,
            'nacl_untrusted_build': 1,
            'nlib_target': 'libppapi_ipc_nacl.a',
            'build_glibc': 0,
            'build_newlib': 0,
            'build_irt': 1,
            'build_pnacl_newlib': 0,
            'build_nonsfi_helper': 1,
          },
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            '../base/base_nacl.gyp:base_nacl',
            '../base/base_nacl.gyp:base_nacl_nonsfi',
            '../gpu/gpu_nacl.gyp:gpu_ipc_nacl',
            '../ipc/ipc_nacl.gyp:ipc_nacl',
            '../ipc/ipc_nacl.gyp:ipc_nacl_nonsfi',
            '../ppapi/ppapi_shared_nacl.gyp:ppapi_shared_nacl',
            '../components/tracing_nacl.gyp:tracing_nacl',
          ],
        },
      ],
    }],
  ],
}
