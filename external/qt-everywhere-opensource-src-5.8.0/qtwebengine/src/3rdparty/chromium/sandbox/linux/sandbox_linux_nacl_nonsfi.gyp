# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../../build/common_untrusted.gypi',
  ],
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'sandbox_linux_nacl_nonsfi',
          'type': 'none',
          'variables': {
            'nacl_untrusted_build': 1,
            'nlib_target': 'libsandbox_linux_nacl_nonsfi.a',
            'build_glibc': 0,
            'build_newlib': 0,
            'build_irt': 0,
            'build_pnacl_newlib': 0,
            'build_nonsfi_helper': 1,
            'compile_flags': [
              '-fgnu-inline-asm',
            ],
            'sources': [
              # This is the subset of linux build target, needed for
              # nacl_helper_nonsfi's sandbox implementation.
              'bpf_dsl/bpf_dsl.cc',
              'bpf_dsl/codegen.cc',
              'bpf_dsl/policy.cc',
              'bpf_dsl/policy_compiler.cc',
              'bpf_dsl/syscall_set.cc',
              'seccomp-bpf-helpers/sigsys_handlers.cc',
              'seccomp-bpf-helpers/syscall_parameters_restrictions.cc',
              'seccomp-bpf/die.cc',
              'seccomp-bpf/sandbox_bpf.cc',
              'seccomp-bpf/syscall.cc',
              'seccomp-bpf/trap.cc',
              'services/credentials.cc',
              'services/namespace_sandbox.cc',
              'services/namespace_utils.cc',
              'services/proc_util.cc',
              'services/resource_limits.cc',
              'services/syscall_wrappers.cc',
              'services/thread_helpers.cc',
              'suid/client/setuid_sandbox_client.cc',
            ],
          },
          'dependencies': [
            '../../base/base_nacl.gyp:base_nacl_nonsfi',
          ],
        },
      ],
    }],

    ['disable_nacl==0 and disable_nacl_untrusted==0 and enable_nacl_nonsfi_test==1', {
      'targets': [
        {
          'target_name': 'sandbox_linux_test_utils_nacl_nonsfi',
          'type': 'none',
          'variables': {
            'nacl_untrusted_build': 1,
            'nlib_target': 'libsandbox_linux_test_utils_nacl_nonsfi.a',
            'build_glibc': 0,
            'build_newlib': 0,
            'build_irt': 0,
            'build_pnacl_newlib': 0,
            'build_nonsfi_helper': 1,

            'sources': [
              'seccomp-bpf/sandbox_bpf_test_runner.cc',
              'tests/sandbox_test_runner.cc',
              'tests/unit_tests.cc',
            ],
          },
          'dependencies': [
            '../../testing/gtest_nacl.gyp:gtest_nacl',
          ],
        },
      ],
    }],
  ],
}
