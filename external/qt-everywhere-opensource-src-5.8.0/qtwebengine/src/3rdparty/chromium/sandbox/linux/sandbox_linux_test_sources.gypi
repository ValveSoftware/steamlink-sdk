# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Tests need to be compiled in the same link unit, so we have to list them
# in a separate .gypi file.
{
  'dependencies': [
    'sandbox',
    'sandbox_linux_test_utils',
    'sandbox_services',
    '../base/base.gyp:base',
    '../testing/gtest.gyp:gtest',
  ],
  'include_dirs': [
    '../..',
  ],
  'sources': [
    'services/proc_util_unittest.cc',
    'services/scoped_process_unittest.cc',
    'services/resource_limits_unittests.cc',
    'services/syscall_wrappers_unittest.cc',
    'services/thread_helpers_unittests.cc',
    'services/yama_unittests.cc',
    'syscall_broker/broker_file_permission_unittest.cc',
    'syscall_broker/broker_process_unittest.cc',
    'tests/main.cc',
    'tests/scoped_temporary_file.cc',
    'tests/scoped_temporary_file.h',
    'tests/scoped_temporary_file_unittest.cc',
    'tests/test_utils_unittest.cc',
    'tests/unit_tests_unittest.cc',
  ],
  'conditions': [
    [ 'compile_suid_client==1', {
      'sources': [
        'suid/client/setuid_sandbox_client_unittest.cc',
        'suid/client/setuid_sandbox_host_unittest.cc',
      ],
    }],
    [ 'use_seccomp_bpf==1', {
      'sources': [
        'bpf_dsl/bpf_dsl_unittest.cc',
        'bpf_dsl/codegen_unittest.cc',
        'bpf_dsl/cons_unittest.cc',
        'bpf_dsl/dump_bpf.cc',
        'bpf_dsl/dump_bpf.h',
        'bpf_dsl/syscall_set_unittest.cc',
        'bpf_dsl/test_trap_registry.cc',
        'bpf_dsl/test_trap_registry.h',
        'bpf_dsl/test_trap_registry_unittest.cc',
        'bpf_dsl/verifier.cc',
        'bpf_dsl/verifier.h',
        'integration_tests/bpf_dsl_seccomp_unittest.cc',
        'integration_tests/seccomp_broker_process_unittest.cc',
        'seccomp-bpf-helpers/baseline_policy_unittest.cc',
        'seccomp-bpf-helpers/syscall_parameters_restrictions_unittests.cc',
        'seccomp-bpf/bpf_tests_unittest.cc',
        'seccomp-bpf/sandbox_bpf_unittest.cc',
        'seccomp-bpf/syscall_unittest.cc',
        'seccomp-bpf/trap_unittest.cc',
      ],
      'dependencies': [
        'bpf_dsl_golden',
      ],
    }],
    [ 'compile_credentials==1', {
      'sources': [
        'integration_tests/namespace_unix_domain_socket_unittest.cc',
        'services/credentials_unittest.cc',
        'services/namespace_utils_unittest.cc',
      ],
      'dependencies': [
        '../build/linux/system.gyp:libcap'
      ],
      'conditions': [
        [ 'use_base_test_suite==1', {
          'sources': [
            'services/namespace_sandbox_unittest.cc',
          ]
        }]
      ],
    }],
    [ 'use_base_test_suite==1', {
      'dependencies': [
        '../base/base.gyp:test_support_base',
      ],
      'defines': [
        'SANDBOX_USES_BASE_TEST_SUITE',
      ],
    }],
  ],
}
