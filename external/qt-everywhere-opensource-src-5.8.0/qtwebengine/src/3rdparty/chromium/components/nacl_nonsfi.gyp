# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/common_untrusted.gypi',
  ],
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          # nacl_helper_nonsfi is similar to nacl_helper (built in nacl.gyp)
          # but for the NaCl plugin in Non-SFI mode.
          # This binary is built using the PNaCl toolchain, but it is native
          # linux binary and will run on Linux directly.
          # Most library code can be shared with the one for untrusted build
          # (i.e. the one for irt.nexe built by the NaCl/PNaCl toolchain), but
          # as nacl_helper_nonsfi runs on Linux, there are some differences,
          # such as MessageLoopForIO (which is based on libevent in Non-SFI
          # mode) or ipc_channel implementation.
          # Because of the toolchain, in both builds, OS_NACL macro (derived
          # from __native_client__ macro) is defined. Code can test whether
          # __native_client_nonsfi__ is #defined in order to determine
          # whether it is being compiled for SFI mode or Non-SFI mode.
          #
          # Currently, nacl_helper_nonsfi is under development and the binary
          # does nothing (i.e. it has only empty main(), now).
          # TODO(crbug.com/358465): Implement it then switch nacl_helper in
          # Non-SFI mode to nacl_helper_nonsfi.
          'target_name': 'nacl_helper_nonsfi',
          'type': 'none',
          'variables': {
            'nacl_untrusted_build': 1,
            'nexe_target': 'nacl_helper_nonsfi',
            # Rename the output binary file to nacl_helper_nonsfi and put it
            # directly under out/{Debug,Release}/.
            'out_newlib32_nonsfi': '<(PRODUCT_DIR)/nacl_helper_nonsfi',
            'out_newlib_arm_nonsfi': '<(PRODUCT_DIR)/nacl_helper_nonsfi',

            'build_glibc': 0,
            'build_newlib': 0,
            'build_irt': 0,
            'build_pnacl_newlib': 0,
            'build_nonsfi_helper': 1,

            'sources': [
              'nacl/common/nacl_messages.cc',
              'nacl/common/nacl_switches.cc',
              'nacl/common/nacl_types.cc',
              'nacl/common/nacl_types_param_traits.cc',
              'nacl/loader/nacl_helper_linux.cc',
              'nacl/loader/nacl_trusted_listener.cc',
              'nacl/loader/nonsfi/nonsfi_listener.cc',
              'nacl/loader/nonsfi/nonsfi_main.cc',
            ],

            'link_flags': [
              '-lbase_nacl_nonsfi',
              '-lcommand_buffer_client_nacl',
              '-lcommand_buffer_common_nacl',
              '-lcontent_common_nacl_nonsfi',
              '-lelf_loader',
              '-levent_nacl_nonsfi',
              '-lgio',
              '-lgles2_cmd_helper_nacl',
              '-lgles2_implementation_nacl',
              '-lgles2_utils_nacl',
              '-lgpu_ipc_nacl',
              '-lipc_nacl_nonsfi',
              '-lmojo_cpp_bindings_nacl',
              '-lmojo_cpp_system_nacl',
              '-lmojo_public_system_nacl',
              '-lmojo_system_impl_nacl_nonsfi',
              '-lnacl_helper_nonsfi_sandbox',
              '-lplatform',
              '-lppapi_ipc_nacl',
              '-lppapi_proxy_nacl',
              '-lppapi_shared_nacl',
              '-lsandbox_linux_nacl_nonsfi',
              '-lshared_memory_support_nacl',
              '-ltracing_nacl',
            ],

            'conditions': [
              ['target_arch=="ia32" or target_arch=="x64"', {
                'extra_deps_newlib32_nonsfi': [
                  '>(tc_lib_dir_nonsfi_helper32)/libbase_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libcommand_buffer_client_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libcommand_buffer_common_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libcontent_common_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libelf_loader.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libevent_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libgio.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libgles2_cmd_helper_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libgles2_implementation_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libgles2_utils_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libgpu_ipc_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libipc_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libmojo_cpp_bindings_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libmojo_cpp_system_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libmojo_public_system_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libmojo_system_impl_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libnacl_helper_nonsfi_sandbox.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libplatform.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libppapi_ipc_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libppapi_proxy_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libppapi_shared_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libsandbox_linux_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libshared_memory_support_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libtracing_nacl.a',
                ],
              }],
              ['target_arch=="arm"', {
                'extra_deps_newlib_arm_nonsfi': [
                  '>(tc_lib_dir_nonsfi_helper_arm)/libbase_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libcommand_buffer_client_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libcommand_buffer_common_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libcontent_common_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libelf_loader.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libevent_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libgio.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libgles2_cmd_helper_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libgles2_implementation_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libgles2_utils_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libgpu_ipc_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libipc_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libmojo_cpp_bindings_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libmojo_cpp_system_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libmojo_public_system_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libmojo_system_impl_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libnacl_helper_nonsfi_sandbox.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libplatform.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libppapi_ipc_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libppapi_proxy_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libppapi_shared_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libsandbox_linux_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libshared_memory_support_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libtracing_nacl.a',
                ],
              }],
            ],
          },
          'dependencies': [
            '../base/base_nacl.gyp:base_nacl_nonsfi',
            '../content/content_nacl_nonsfi.gyp:content_common_nacl_nonsfi',
            '../ipc/ipc_nacl.gyp:ipc_nacl_nonsfi',
            '../mojo/mojo_edk_nacl.gyp:mojo_system_impl_nacl_nonsfi',
            '../mojo/mojo_public_nacl.gyp:mojo_cpp_bindings_nacl',
            '../mojo/mojo_public_nacl.gyp:mojo_cpp_system_nacl',
            '../mojo/mojo_public_nacl.gyp:mojo_public_system_nacl',
            '../native_client/src/nonsfi/irt/irt.gyp:nacl_sys_private',
            '../native_client/src/nonsfi/loader/loader.gyp:elf_loader',
            '../native_client/src/untrusted/nacl/nacl.gyp:nacl_lib_newlib',
            '../ppapi/ppapi_proxy_nacl.gyp:ppapi_proxy_nacl',
            '../sandbox/linux/sandbox_linux_nacl_nonsfi.gyp:sandbox_linux_nacl_nonsfi',
            'nacl_helper_nonsfi_sandbox',
          ],
        },

        {
          'target_name': 'nacl_helper_nonsfi_sandbox',
          'type': 'none',
          'variables': {
            'nacl_untrusted_build': 1,
            'nlib_target': 'libnacl_helper_nonsfi_sandbox.a',

            'build_glibc': 0,
            'build_newlib': 0,
            'build_irt': 0,
            'build_pnacl_newlib': 0,
            'build_nonsfi_helper': 1,

            'sources': [
              'nacl/loader/nonsfi/nonsfi_sandbox.cc',
              'nacl/loader/sandbox_linux/nacl_sandbox_linux.cc',
            ],
          },
          'dependencies': [
            '../base/base_nacl.gyp:base_nacl_nonsfi',
            '../content/content_nacl_nonsfi.gyp:content_common_nacl_nonsfi',
            '../sandbox/linux/sandbox_linux_nacl_nonsfi.gyp:sandbox_linux_nacl_nonsfi',
          ],
        },
      ],
    }],

    ['disable_nacl==0 and disable_nacl_untrusted==0 and enable_nacl_nonsfi_test==1', {
      'targets': [
        {
          'target_name': 'nacl_helper_nonsfi_unittests_main',
          'type': 'none',
          'variables': {
            'nacl_untrusted_build': 1,
            'nexe_target': 'nacl_helper_nonsfi_unittests_main',
            # Rename the output binary file to
            # nacl_helper_nonsfi_unittests_main and put it directly under
            # out/{Debug,Release}/, so that this is in the standard location,
            # for running on the buildbots.
            'out_newlib32_nonsfi': '<(PRODUCT_DIR)/nacl_helper_nonsfi_unittests_main',
            'out_newlib_arm_nonsfi': '<(PRODUCT_DIR)/nacl_helper_nonsfi_unittests_main',

            'build_glibc': 0,
            'build_newlib': 0,
            'build_irt': 0,
            'build_pnacl_newlib': 0,
            'build_nonsfi_helper': 1,

            'sources': [
              'nacl/loader/nonsfi/nonsfi_sandbox_sigsys_unittest.cc',
              'nacl/loader/nonsfi/nonsfi_sandbox_unittest.cc',
              'nacl/loader/nonsfi/run_all_unittests.cc',
            ],

            'link_flags': [
              '-lbase_nacl_nonsfi',
              '-lcontent_common_nacl_nonsfi',
              '-levent_nacl_nonsfi',
              '-lgio',
              '-lgtest_nacl',
              '-lnacl_helper_nonsfi_sandbox',
              '-lplatform',
              '-lsandbox_linux_nacl_nonsfi',
              '-lsandbox_linux_test_utils_nacl_nonsfi',
              '-ltest_support_base_nacl_nonsfi',
            ],

            'conditions': [
              ['target_arch=="ia32" or target_arch=="x64"', {
                'extra_deps_newlib32_nonsfi': [
                  '>(tc_lib_dir_nonsfi_helper32)/libbase_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libcontent_common_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libevent_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libgio.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libgtest_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libnacl_helper_nonsfi_sandbox.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libplatform.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libsandbox_linux_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libsandbox_linux_test_utils_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libtest_support_base_nacl_nonsfi.a',
                ],
              }],
              ['target_arch=="arm"', {
                'extra_deps_newlib_arm_nonsfi': [
                  '>(tc_lib_dir_nonsfi_helper_arm)/libbase_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libcontent_common_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libevent_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libgio.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libgtest_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libnacl_helper_nonsfi_sandbox.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libplatform.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libsandbox_linux_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libsandbox_linux_test_utils_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libtest_support_base_nacl_nonsfi.a',
                ],
              }],
            ],
          },

          'dependencies': [
            '../base/base_nacl.gyp:base_nacl_nonsfi',
            '../base/base_nacl.gyp:test_support_base_nacl_nonsfi',
            '../content/content_nacl_nonsfi.gyp:content_common_nacl_nonsfi',
            '../native_client/src/nonsfi/irt/irt.gyp:nacl_sys_private',
            '../native_client/src/untrusted/nacl/nacl.gyp:nacl_lib_newlib',
            '../sandbox/linux/sandbox_linux_nacl_nonsfi.gyp:sandbox_linux_nacl_nonsfi',
            '../sandbox/linux/sandbox_linux_nacl_nonsfi.gyp:sandbox_linux_test_utils_nacl_nonsfi',
            '../testing/gtest_nacl.gyp:gtest_nacl',
            'nacl_helper_nonsfi_sandbox',
          ],
        },
      ],
    }],

    ['disable_nacl==0 and disable_nacl_untrusted==0 and enable_nacl_nonsfi_test==1 and test_isolation_mode!="noop"', {
      'targets': [
        {
          'target_name': 'nacl_helper_nonsfi_unittests_run',
          'type': 'none',
          'dependencies': [
            'nacl.gyp:nacl_helper_nonsfi_unittests',
            'nacl_helper_nonsfi_unittests_main',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'nacl_helper_nonsfi_unittests.isolate',
          ],
        },
      ],
    }],
  ],
}
