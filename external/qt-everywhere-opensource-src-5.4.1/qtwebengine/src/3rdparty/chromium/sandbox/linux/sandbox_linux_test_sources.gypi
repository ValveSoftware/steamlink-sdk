# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Tests need to be compiled in the same link unit, so we have to list them
# in a separate .gypi file.
{
  'dependencies': [
    'sandbox',
    'sandbox_linux_test_utils',
    '../base/base.gyp:base',
    '../base/base.gyp:test_support_base',
    '../testing/gtest.gyp:gtest',
  ],
  'include_dirs': [
    '../..',
  ],
  'sources': [
    'tests/main.cc',
    'tests/unit_tests_unittest.cc',
    'services/broker_process_unittest.cc',
    'services/scoped_process_unittest.cc',
    'services/thread_helpers_unittests.cc',
    'services/yama_unittests.cc',
  ],
  'conditions': [
    [ 'compile_suid_client==1', {
      'sources': [
        'suid/client/setuid_sandbox_client_unittest.cc',
      ],
    }],
    [ 'use_seccomp_bpf==1', {
      'sources': [
        'seccomp-bpf-helpers/baseline_policy_unittest.cc',
        'seccomp-bpf/bpf_tests_unittest.cc',
        'seccomp-bpf/codegen_unittest.cc',
        'seccomp-bpf/errorcode_unittest.cc',
        'seccomp-bpf/sandbox_bpf_unittest.cc',
        'seccomp-bpf/syscall_iterator_unittest.cc',
        'seccomp-bpf/syscall_unittest.cc',
      ],
    }],
    [ 'compile_credentials==1', {
      'sources': [
        'services/credentials_unittest.cc',
        'services/unix_domain_socket_unittest.cc',
      ],
    }],
  ],
}
