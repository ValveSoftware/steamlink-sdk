# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/common_untrusted.gypi',
  ],
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'ppapi_lib',
          'type': 'none',
          'dependencies': [
             '../../native_client/src/untrusted/pthread/pthread.gyp:pthread_lib',
             'src/untrusted/irt_stub/irt_stub.gyp:ppapi_stub_lib',
          ],
          'include_dirs': [
            '..',
          ],
          'copies': [
            {
              'destination': '>(tc_include_dir_newlib)/nacl',
              'files': [
                'src/shared/ppapi_proxy/ppruntime.h',
              ],
            },

            {
              'destination': '>(tc_lib_dir_pnacl_newlib)',
              'files': [
                'src/untrusted/irt_stub/libppapi.a',
              ],
            },
          ],
          'conditions': [
            ['target_arch=="ia32" or target_arch=="x64"', {
              'copies': [
                {
                  'destination': '>(tc_include_dir_glibc)/include/nacl',
                  'files': [
                    'src/shared/ppapi_proxy/ppruntime.h',
                  ],
                },
              ],
            }],
            ['target_arch=="ia32"', {
              'copies': [
                # Here we copy linker scripts out of the Native Client repo..
                # These are source, not build artifacts.
                {
                  'destination': '>(tc_lib_dir_newlib32)',
                  'files': [
                    'src/untrusted/irt_stub/libppapi.a',
                  ],
                },
                {
                  'destination': '>(tc_lib_dir_glibc32)',
                  'files': [
                    'src/untrusted/irt_stub/libppapi.a',
                    'src/untrusted/irt_stub/libppapi.so',
                  ],
                },
              ],
            }],
            ['target_arch=="x64" or (target_arch=="ia32" and OS=="win")', {
              'copies': [
                {
                  'destination': '>(tc_lib_dir_newlib64)',
                  'files': [
                    'src/untrusted/irt_stub/libppapi.a',
                  ],
                },
                {
                  'destination': '>(tc_lib_dir_glibc64)',
                  'files': [
                    'src/untrusted/irt_stub/libppapi.a',
                    'src/untrusted/irt_stub/libppapi.so',
                  ],
                },
              ]
            }],
            ['target_arch=="arm"', {
              'copies': [
                {
                  'destination': '>(tc_lib_dir_newlib_arm)',
                  'files': [
                    'src/untrusted/irt_stub/libppapi.a',
                  ],
                },
                {
                  'destination': '>(tc_lib_dir_glibc_arm)',
                  'files': [
                    'src/untrusted/irt_stub/libppapi.a',
                    'src/untrusted/irt_stub/libppapi.so',
                  ],
                },
              ]
            }],
            ['target_arch=="mipsel"', {
              'copies': [
                {
                  'destination': '>(tc_lib_dir_newlib_mips)',
                  'files': [
                    'src/untrusted/irt_stub/libppapi.a',
                  ],
                },
              ]
            }]
          ],
        },
        {
          'target_name': 'nacl_irt',
          'type': 'none',
          'variables': {
            'nexe_target': 'nacl_irt',
            # These out_* fields override the default filenames, which
            # include a "_newlib" suffix and places them in the target
            # directory.
            'out_newlib64': '<(PRODUCT_DIR)/nacl_irt_x86_64.nexe',
            'out_newlib32': '<(PRODUCT_DIR)/nacl_irt_x86_32.nexe',
            'out_newlib_arm': '<(PRODUCT_DIR)/nacl_irt_arm.nexe',
            'out_newlib_mips': '<(PRODUCT_DIR)/nacl_irt_mips32.nexe',
            'build_glibc': 0,
            'build_newlib': 0,
            'build_irt': 1,
            'include_dirs': [
              '..',
            ],
            'link_flags': [
              '-Wl,--start-group',
              '-lirt_browser',
              '-lpnacl_irt_shim_for_irt',
              '-lppapi_proxy_nacl',
              '-lppapi_ipc_nacl',
              '-lppapi_shared_nacl',
              '-lgles2_implementation_nacl',
              '-lgles2_cmd_helper_nacl',
              '-lgles2_utils_nacl',
              '-lcommand_buffer_client_nacl',
              '-lcommand_buffer_common_nacl',
              '-ltracing_nacl',
              '-lgpu_ipc_nacl',
              '-lipc_nacl',
              '-lbase_nacl',
              '-lshared_memory_support_nacl',
              '-limc_syscalls',
              '-lplatform',
              '-lgio',
              '-lmojo_cpp_bindings_nacl',
              '-lmojo_cpp_system_nacl',
              '-lmojo_public_system_nacl',
              '-lmojo_system_impl_nacl',
              '-Wl,--end-group',
              '-lm',
            ],
            'extra_args': [
              '--strip-all',
            ],
            'conditions': [
              # untrusted.gypi and build_nexe.py currently build
              # both x86-32 and x86-64 whenever target_arch is some
              # flavor of x86.  However, on non-windows platforms
              # we only need one architecture.
              ['OS!="win" and target_arch=="ia32"',
                {
                  'enable_x86_64': 0
                }
              ],
              ['target_arch=="x64"',
                {
                  'enable_x86_32': 0
                }
              ],
              ['target_arch=="ia32" or target_arch=="x64"', {
                'extra_deps_newlib64': [
                  '>(tc_lib_dir_irt64)/libppapi_proxy_nacl.a',
                  '>(tc_lib_dir_irt64)/libppapi_ipc_nacl.a',
                  '>(tc_lib_dir_irt64)/libppapi_shared_nacl.a',
                  '>(tc_lib_dir_irt64)/libgles2_implementation_nacl.a',
                  '>(tc_lib_dir_irt64)/libcommand_buffer_client_nacl.a',
                  '>(tc_lib_dir_irt64)/libcommand_buffer_common_nacl.a',
                  '>(tc_lib_dir_irt64)/libgpu_ipc_nacl.a',
                  '>(tc_lib_dir_irt64)/libtracing_nacl.a',
                  '>(tc_lib_dir_irt64)/libgles2_cmd_helper_nacl.a',
                  '>(tc_lib_dir_irt64)/libgles2_utils_nacl.a',
                  '>(tc_lib_dir_irt64)/libipc_nacl.a',
                  '>(tc_lib_dir_irt64)/libbase_nacl.a',
                  '>(tc_lib_dir_irt64)/libirt_browser.a',
                  '>(tc_lib_dir_irt64)/libpnacl_irt_shim_for_irt.a',
                  '>(tc_lib_dir_irt64)/libshared_memory_support_nacl.a',
                  '>(tc_lib_dir_irt64)/libplatform.a',
                  '>(tc_lib_dir_irt64)/libimc_syscalls.a',
                  '>(tc_lib_dir_irt64)/libgio.a',
                  '>(tc_lib_dir_irt64)/libmojo_cpp_bindings_nacl.a',
                  '>(tc_lib_dir_irt64)/libmojo_cpp_system_nacl.a',
                  '>(tc_lib_dir_irt64)/libmojo_public_system_nacl.a',
                  '>(tc_lib_dir_irt64)/libmojo_system_impl_nacl.a',
                ],
                'extra_deps_newlib32': [
                  '>(tc_lib_dir_irt32)/libppapi_proxy_nacl.a',
                  '>(tc_lib_dir_irt32)/libppapi_ipc_nacl.a',
                  '>(tc_lib_dir_irt32)/libppapi_shared_nacl.a',
                  '>(tc_lib_dir_irt32)/libgles2_implementation_nacl.a',
                  '>(tc_lib_dir_irt32)/libcommand_buffer_client_nacl.a',
                  '>(tc_lib_dir_irt32)/libcommand_buffer_common_nacl.a',
                  '>(tc_lib_dir_irt32)/libgpu_ipc_nacl.a',
                  '>(tc_lib_dir_irt32)/libtracing_nacl.a',
                  '>(tc_lib_dir_irt32)/libgles2_cmd_helper_nacl.a',
                  '>(tc_lib_dir_irt32)/libgles2_utils_nacl.a',
                  '>(tc_lib_dir_irt32)/libipc_nacl.a',
                  '>(tc_lib_dir_irt32)/libbase_nacl.a',
                  '>(tc_lib_dir_irt32)/libirt_browser.a',
                  '>(tc_lib_dir_irt32)/libpnacl_irt_shim_for_irt.a',
                  '>(tc_lib_dir_irt32)/libshared_memory_support_nacl.a',
                  '>(tc_lib_dir_irt32)/libplatform.a',
                  '>(tc_lib_dir_irt32)/libimc_syscalls.a',
                  '>(tc_lib_dir_irt32)/libgio.a',
                  '>(tc_lib_dir_irt32)/libmojo_cpp_bindings_nacl.a',
                  '>(tc_lib_dir_irt32)/libmojo_cpp_system_nacl.a',
                  '>(tc_lib_dir_irt32)/libmojo_public_system_nacl.a',
                  '>(tc_lib_dir_irt32)/libmojo_system_impl_nacl.a',
                ],
              }],
              ['target_arch=="arm"', {
                'extra_deps_arm': [
                  '>(tc_lib_dir_irt_arm)/libppapi_proxy_nacl.a',
                  '>(tc_lib_dir_irt_arm)/libppapi_ipc_nacl.a',
                  '>(tc_lib_dir_irt_arm)/libppapi_shared_nacl.a',
                  '>(tc_lib_dir_irt_arm)/libgles2_implementation_nacl.a',
                  '>(tc_lib_dir_irt_arm)/libcommand_buffer_client_nacl.a',
                  '>(tc_lib_dir_irt_arm)/libcommand_buffer_common_nacl.a',
                  '>(tc_lib_dir_irt_arm)/libgpu_ipc_nacl.a',
                  '>(tc_lib_dir_irt_arm)/libtracing_nacl.a',
                  '>(tc_lib_dir_irt_arm)/libgles2_cmd_helper_nacl.a',
                  '>(tc_lib_dir_irt_arm)/libgles2_utils_nacl.a',
                  '>(tc_lib_dir_irt_arm)/libipc_nacl.a',
                  '>(tc_lib_dir_irt_arm)/libbase_nacl.a',
                  '>(tc_lib_dir_irt_arm)/libirt_browser.a',
                  '>(tc_lib_dir_irt_arm)/libpnacl_irt_shim_for_irt.a',
                  '>(tc_lib_dir_irt_arm)/libshared_memory_support_nacl.a',
                  '>(tc_lib_dir_irt_arm)/libplatform.a',
                  '>(tc_lib_dir_irt_arm)/libimc_syscalls.a',
                  '>(tc_lib_dir_irt_arm)/libgio.a',
                  '>(tc_lib_dir_irt_arm)/libmojo_cpp_bindings_nacl.a',
                  '>(tc_lib_dir_irt_arm)/libmojo_cpp_system_nacl.a',
                  '>(tc_lib_dir_irt_arm)/libmojo_public_system_nacl.a',
                  '>(tc_lib_dir_irt_arm)/libmojo_system_impl_nacl.a',
                ],
              }],
              ['target_arch=="mipsel"', {
                'extra_deps_mips': [
                  '>(tc_lib_dir_irt_mips)/libppapi_proxy_nacl.a',
                  '>(tc_lib_dir_irt_mips)/libppapi_ipc_nacl.a',
                  '>(tc_lib_dir_irt_mips)/libppapi_shared_nacl.a',
                  '>(tc_lib_dir_irt_mips)/libgles2_implementation_nacl.a',
                  '>(tc_lib_dir_irt_mips)/libcommand_buffer_client_nacl.a',
                  '>(tc_lib_dir_irt_mips)/libcommand_buffer_common_nacl.a',
                  '>(tc_lib_dir_irt_mips)/libgpu_ipc_nacl.a',
                  '>(tc_lib_dir_irt_mips)/libtracing_nacl.a',
                  '>(tc_lib_dir_irt_mips)/libgles2_cmd_helper_nacl.a',
                  '>(tc_lib_dir_irt_mips)/libgles2_utils_nacl.a',
                  '>(tc_lib_dir_irt_mips)/libipc_nacl.a',
                  '>(tc_lib_dir_irt_mips)/libbase_nacl.a',
                  '>(tc_lib_dir_irt_mips)/libirt_browser.a',
                  '>(tc_lib_dir_irt_mips)/libpnacl_irt_shim_for_irt.a',
                  '>(tc_lib_dir_irt_mips)/libshared_memory_support_nacl.a',
                  '>(tc_lib_dir_irt_mips)/libplatform.a',
                  '>(tc_lib_dir_irt_mips)/libimc_syscalls.a',
                  '>(tc_lib_dir_irt_mips)/libgio.a',
                  '>(tc_lib_dir_irt_mips)/libmojo_cpp_bindings_nacl.a',
                  '>(tc_lib_dir_irt_mips)/libmojo_cpp_system_nacl.a',
                  '>(tc_lib_dir_irt_mips)/libmojo_public_system_nacl.a',
                  '>(tc_lib_dir_irt_mips)/libmojo_system_impl_nacl.a',
                ],
              }],
            ],
          },
          'dependencies': [
            'src/untrusted/pnacl_irt_shim/pnacl_irt_shim.gyp:irt',
            '../ppapi_proxy_nacl.gyp:ppapi_proxy_nacl',
            '../ppapi_ipc_nacl.gyp:ppapi_ipc_nacl',
            '../ppapi_shared_nacl.gyp:ppapi_shared_nacl',
            '../../gpu/command_buffer/command_buffer_nacl.gyp:gles2_utils_nacl',
            '../../gpu/gpu_nacl.gyp:command_buffer_client_nacl',
            '../../gpu/gpu_nacl.gyp:command_buffer_common_nacl',
            '../../gpu/gpu_nacl.gyp:gles2_implementation_nacl',
            '../../gpu/gpu_nacl.gyp:gles2_cmd_helper_nacl',
            '../../gpu/gpu_nacl.gyp:gpu_ipc_nacl',
            '../../components/tracing_nacl.gyp:tracing_nacl',
            '../../ipc/ipc_nacl.gyp:ipc_nacl',
            '../../base/base_nacl.gyp:base_nacl',
            '../../media/media_nacl.gyp:shared_memory_support_nacl',
            '../../mojo/mojo_edk_nacl.gyp:mojo_system_impl_nacl',
            '../../mojo/mojo_public_nacl.gyp:mojo_cpp_bindings_nacl',
            '../../mojo/mojo_public_nacl.gyp:mojo_cpp_system_nacl',
            '../../mojo/mojo_public_nacl.gyp:mojo_public_system_nacl',
            '../../native_client/src/untrusted/irt/irt.gyp:irt_browser_lib',
            '../../native_client/src/shared/platform/platform.gyp:platform_lib',
            '../../native_client/src/tools/tls_edit/tls_edit.gyp:tls_edit#host',
            '../../native_client/src/untrusted/nacl/nacl.gyp:imc_syscalls_lib',
            '../../native_client/src/shared/gio/gio.gyp:gio_lib',
          ],
        },
      ],
    }],
  ],
}
