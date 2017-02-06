# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/common_untrusted.gypi',
    'ppapi_proxy.gypi',
  ],
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'ppapi_proxy_nacl',
          'type': 'none',
          'variables': {
            'ppapi_proxy_target': 1,
            'nacl_untrusted_build': 1,
            'nlib_target': 'libppapi_proxy_nacl.a',
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
            '../components/tracing_nacl.gyp:tracing_nacl',
            '../gpu/command_buffer/command_buffer_nacl.gyp:gles2_utils_nacl',
            '../gpu/gpu_nacl.gyp:command_buffer_client_nacl',
            '../gpu/gpu_nacl.gyp:command_buffer_common_nacl',
            '../gpu/gpu_nacl.gyp:gles2_cmd_helper_nacl',
            '../gpu/gpu_nacl.gyp:gles2_implementation_nacl',
            '../gpu/gpu_nacl.gyp:gpu_ipc_nacl',
            '../ipc/ipc_nacl.gyp:ipc_nacl',
            '../ipc/ipc_nacl.gyp:ipc_nacl_nonsfi',
            '../mojo/mojo_edk_nacl.gyp:mojo_system_impl_nacl',
            '../mojo/mojo_edk_nacl.gyp:mojo_system_impl_nacl_nonsfi',
            '../ppapi/ppapi_ipc_nacl.gyp:ppapi_ipc_nacl',
            '../ppapi/ppapi_shared_nacl.gyp:ppapi_shared_nacl',
            '../third_party/WebKit/public/blink_headers.gyp:blink_headers',
            '../third_party/khronos/khronos.gyp:khronos_headers',
          ],
        },
      ],
    }],
  ],
}
