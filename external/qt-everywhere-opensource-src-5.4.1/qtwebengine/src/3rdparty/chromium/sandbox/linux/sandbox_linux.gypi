# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      ['OS=="linux"', {
        'compile_suid_client': 1,
        'compile_credentials': 1,
      }, {
        'compile_suid_client': 0,
        'compile_credentials': 0,
      }],
      ['OS=="linux" and (target_arch=="ia32" or target_arch=="x64")', {
        'compile_seccomp_bpf_demo': 1,
      }, {
        'compile_seccomp_bpf_demo': 0,
      }],
    ],
  },
  'target_defaults': {
    'target_conditions': [
      # All linux/ files will automatically be excluded on Android
      # so make sure we re-include them explicitly.
      ['OS == "android"', {
        'sources/': [
          ['include', '^linux/'],
        ],
      }],
    ],
  },
  'targets': [
    # We have two principal targets: sandbox and sandbox_linux_unittests
    # All other targets are listed as dependencies.
    # There is one notable exception: for historical reasons, chrome_sandbox is
    # the setuid sandbox and is its own target.
    {
      'target_name': 'sandbox',
      'type': 'none',
      'dependencies': [
        'sandbox_services',
      ],
      'conditions': [
        [ 'compile_suid_client==1', {
          'dependencies': [
            'suid_sandbox_client',
          ],
        }],
        # Compile seccomp BPF when we support it.
        [ 'use_seccomp_bpf==1', {
          'dependencies': [
            'seccomp_bpf',
            'seccomp_bpf_helpers',
          ],
        }],
      ],
    },
    {
      'target_name': 'sandbox_linux_test_utils',
      'type': 'static_library',
      'dependencies': [
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'tests/sandbox_test_runner.cc',
        'tests/sandbox_test_runner.h',
        'tests/sandbox_test_runner_function_pointer.cc',
        'tests/sandbox_test_runner_function_pointer.h',
        'tests/test_utils.cc',
        'tests/test_utils.h',
        'tests/unit_tests.cc',
        'tests/unit_tests.h',
      ],
      'conditions': [
        [ 'use_seccomp_bpf==1', {
          'sources': [
            'seccomp-bpf/bpf_tester_compatibility_delegate.h',
            'seccomp-bpf/bpf_tests.h',
            'seccomp-bpf/sandbox_bpf_test_runner.cc',
            'seccomp-bpf/sandbox_bpf_test_runner.h',
          ],
          'dependencies': [
            'seccomp_bpf',
          ]
        }],
      ],
    },
    {
      # The main sandboxing test target.
      'target_name': 'sandbox_linux_unittests',
      'includes': [
        'sandbox_linux_test_sources.gypi',
      ],
      'type': 'executable',
    },
    {
      # This target is the shared library used by Android APK (i.e.
      # JNI-friendly) tests.
      'target_name': 'sandbox_linux_jni_unittests',
      'includes': [
        'sandbox_linux_test_sources.gypi',
      ],
      'type': 'shared_library',
      'conditions': [
        [ 'OS == "android"', {
          'dependencies': [
            '../testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
      ],
    },
    {
      'target_name': 'seccomp_bpf',
      'type': '<(component)',
      'sources': [
        'seccomp-bpf/basicblock.cc',
        'seccomp-bpf/basicblock.h',
        'seccomp-bpf/codegen.cc',
        'seccomp-bpf/codegen.h',
        'seccomp-bpf/die.cc',
        'seccomp-bpf/die.h',
        'seccomp-bpf/errorcode.cc',
        'seccomp-bpf/errorcode.h',
        'seccomp-bpf/instruction.h',
        'seccomp-bpf/linux_seccomp.h',
        'seccomp-bpf/sandbox_bpf.cc',
        'seccomp-bpf/sandbox_bpf.h',
        'seccomp-bpf/sandbox_bpf_compatibility_policy.h',
        'seccomp-bpf/sandbox_bpf_policy.cc',
        'seccomp-bpf/sandbox_bpf_policy.h',
        'seccomp-bpf/syscall.cc',
        'seccomp-bpf/syscall.h',
        'seccomp-bpf/syscall_iterator.cc',
        'seccomp-bpf/syscall_iterator.h',
        'seccomp-bpf/trap.cc',
        'seccomp-bpf/trap.h',
        'seccomp-bpf/verifier.cc',
        'seccomp-bpf/verifier.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'sandbox_services_headers',
      ],
      'defines': [
        'SANDBOX_IMPLEMENTATION',
      ],
      'include_dirs': [
        '../..',
      ],
    },
    {
      'target_name': 'seccomp_bpf_helpers',
      'type': '<(component)',
      'sources': [
        'seccomp-bpf-helpers/baseline_policy.cc',
        'seccomp-bpf-helpers/baseline_policy.h',
        'seccomp-bpf-helpers/sigsys_handlers.cc',
        'seccomp-bpf-helpers/sigsys_handlers.h',
        'seccomp-bpf-helpers/syscall_parameters_restrictions.cc',
        'seccomp-bpf-helpers/syscall_parameters_restrictions.h',
        'seccomp-bpf-helpers/syscall_sets.cc',
        'seccomp-bpf-helpers/syscall_sets.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'seccomp_bpf',
      ],
      'defines': [
        'SANDBOX_IMPLEMENTATION',
      ],
      'include_dirs': [
        '../..',
      ],
    },
    {
      # A demonstration program for the seccomp-bpf sandbox.
      'target_name': 'seccomp_bpf_demo',
      'conditions': [
        ['compile_seccomp_bpf_demo==1', {
          'type': 'executable',
          'sources': [
            'seccomp-bpf/demo.cc',
          ],
          'dependencies': [
            'seccomp_bpf',
          ],
        }, {
          'type': 'none',
        }],
      ],
      'include_dirs': [
        '../../',
      ],
    },
    {
      # The setuid sandbox, for Linux
      'target_name': 'chrome_sandbox',
      'type': 'executable',
      'sources': [
        'suid/common/sandbox.h',
        'suid/common/suid_unsafe_environment_variables.h',
        'suid/linux_util.c',
        'suid/linux_util.h',
        'suid/process_util.h',
        'suid/process_util_linux.c',
        'suid/sandbox.c',
      ],
      'cflags': [
        # For ULLONG_MAX
        '-std=gnu99',
      ],
      'include_dirs': [
        '../..',
      ],
      # Do not use any sanitizer tools with this binary. http://crbug.com/382766
      'cflags/': [
        ['exclude', '-fsanitize'],
      ],
      'ldflags/': [
        ['exclude', '-fsanitize'],
      ],
    },
    { 'target_name': 'sandbox_services',
      'type': '<(component)',
      'sources': [
        'services/broker_process.cc',
        'services/broker_process.h',
        'services/init_process_reaper.cc',
        'services/init_process_reaper.h',
        'services/scoped_process.cc',
        'services/scoped_process.h',
        'services/thread_helpers.cc',
        'services/thread_helpers.h',
        'services/yama.h',
        'services/yama.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'defines': [
        'SANDBOX_IMPLEMENTATION',
      ],
      'conditions': [
        ['compile_credentials==1', {
          'sources': [
            'services/credentials.cc',
            'services/credentials.h',
          ],
          'dependencies': [
            # for capabilities.cc.
            '../build/linux/system.gyp:libcap',
          ],
        }],
      ],
      'include_dirs': [
        '..',
      ],
    },
    { 'target_name': 'sandbox_services_headers',
      'type': 'none',
      'sources': [
        'services/android_arm_ucontext.h',
        'services/android_futex.h',
        'services/android_ucontext.h',
        'services/android_i386_ucontext.h',
        'services/arm_linux_syscalls.h',
        'services/linux_syscalls.h',
        'services/x86_32_linux_syscalls.h',
        'services/x86_64_linux_syscalls.h',
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
      # We make this its own target so that it does not interfere
      # with our tests.
      'target_name': 'libc_urandom_override',
      'type': 'static_library',
      'sources': [
        'services/libc_urandom_override.cc',
        'services/libc_urandom_override.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
      'target_name': 'suid_sandbox_client',
      'type': '<(component)',
      'sources': [
        'suid/common/sandbox.h',
        'suid/common/suid_unsafe_environment_variables.h',
        'suid/client/setuid_sandbox_client.cc',
        'suid/client/setuid_sandbox_client.h',
      ],
      'defines': [
        'SANDBOX_IMPLEMENTATION',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'sandbox_services',
      ],
      'include_dirs': [
        '..',
      ],
    },
  ],
  'conditions': [
    [ 'OS=="android"', {
      'targets': [
        {
        'target_name': 'sandbox_linux_unittests_stripped',
        'type': 'none',
        'dependencies': [ 'sandbox_linux_unittests' ],
        'actions': [{
          'action_name': 'strip sandbox_linux_unittests',
          'inputs': [ '<(PRODUCT_DIR)/sandbox_linux_unittests' ],
          'outputs': [ '<(PRODUCT_DIR)/sandbox_linux_unittests_stripped' ],
          'action': [ '<(android_strip)', '<@(_inputs)', '-o', '<@(_outputs)' ],
          }],
        }
      ],
    }],
    [ 'OS=="android"', {
      'targets': [
        {
        'target_name': 'sandbox_linux_jni_unittests_apk',
        'type': 'none',
        'variables': {
          'test_suite_name': 'sandbox_linux_jni_unittests',
        },
        'dependencies': [
          'sandbox_linux_jni_unittests',
        ],
        'includes': [ '../../build/apk_test.gypi' ],
        }
      ],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'sandbox_linux_unittests_run',
          'type': 'none',
          'dependencies': [
            'sandbox_linux_unittests',
          ],
          'includes': [
            '../../build/isolate.gypi',
            '../sandbox_linux_unittests.isolate',
          ],
          'sources': [
            '../sandbox_linux_unittests.isolate',
          ],
        },
      ],
    }],
  ],
}
