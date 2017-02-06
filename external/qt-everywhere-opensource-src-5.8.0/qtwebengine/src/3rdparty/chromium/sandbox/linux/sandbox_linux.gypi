# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      ['OS=="linux"', {
        'compile_suid_client': 1,
        'compile_credentials': 1,
        'use_base_test_suite': 1,
      }, {
        'compile_suid_client': 0,
        'compile_credentials': 0,
        'use_base_test_suite': 0,
      }],
      ['OS=="linux" and (target_arch=="ia32" or target_arch=="x64" or '
         'target_arch=="mipsel")', {
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
        [ 'use_base_test_suite==1', {
          'dependencies': [
            '../base/base.gyp:test_support_base',
          ],
          'defines': [
            'SANDBOX_USES_BASE_TEST_SUITE',
          ],
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
      'conditions': [
        [ 'OS == "android"', {
          'variables': {
            'test_type': 'gtest',
            'test_suite_name': '<(_target_name)',
          },
          'includes': [
            '../../build/android/test_runner.gypi',
          ],
        }]
      ]
    },
    {
      'target_name': 'seccomp_bpf',
      'type': '<(component)',
      'sources': [
        'bpf_dsl/bpf_dsl.cc',
        'bpf_dsl/bpf_dsl.h',
        'bpf_dsl/bpf_dsl_forward.h',
        'bpf_dsl/bpf_dsl_impl.h',
        'bpf_dsl/codegen.cc',
        'bpf_dsl/codegen.h',
        'bpf_dsl/cons.h',
        'bpf_dsl/errorcode.h',
        'bpf_dsl/linux_syscall_ranges.h',
        'bpf_dsl/policy.cc',
        'bpf_dsl/policy.h',
        'bpf_dsl/policy_compiler.cc',
        'bpf_dsl/policy_compiler.h',
        'bpf_dsl/seccomp_macros.h',
        'bpf_dsl/seccomp_macros.h',
        'bpf_dsl/syscall_set.cc',
        'bpf_dsl/syscall_set.h',
        'bpf_dsl/trap_registry.h',
        'seccomp-bpf/die.cc',
        'seccomp-bpf/die.h',
        'seccomp-bpf/sandbox_bpf.cc',
        'seccomp-bpf/sandbox_bpf.h',
        'seccomp-bpf/syscall.cc',
        'seccomp-bpf/syscall.h',
        'seccomp-bpf/trap.cc',
        'seccomp-bpf/trap.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'sandbox_services',
        'sandbox_services_headers',
      ],
      'defines': [
        'SANDBOX_IMPLEMENTATION',
      ],
      'includes': [
        # Disable LTO due to compiler bug
        # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=57703
        '../../build/android/disable_gcc_lto.gypi',
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
        'sandbox_services',
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
      # The setuid sandbox, for Linux
      'target_name': 'chrome_sandbox',
      'type': 'executable',
      'sources': [
        'suid/common/sandbox.h',
        'suid/common/suid_unsafe_environment_variables.h',
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
        'services/init_process_reaper.cc',
        'services/init_process_reaper.h',
        'services/proc_util.cc',
        'services/proc_util.h',
        'services/resource_limits.cc',
        'services/resource_limits.h',
        'services/scoped_process.cc',
        'services/scoped_process.h',
        'services/syscall_wrappers.cc',
        'services/syscall_wrappers.h',
        'services/thread_helpers.cc',
        'services/thread_helpers.h',
        'services/yama.cc',
        'services/yama.h',
        'syscall_broker/broker_channel.cc',
        'syscall_broker/broker_channel.h',
        'syscall_broker/broker_client.cc',
        'syscall_broker/broker_client.h',
        'syscall_broker/broker_common.h',
        'syscall_broker/broker_file_permission.cc',
        'syscall_broker/broker_file_permission.h',
        'syscall_broker/broker_host.cc',
        'syscall_broker/broker_host.h',
        'syscall_broker/broker_policy.cc',
        'syscall_broker/broker_policy.h',
        'syscall_broker/broker_process.cc',
        'syscall_broker/broker_process.h',
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
            'services/namespace_sandbox.cc',
            'services/namespace_sandbox.h',
            'services/namespace_utils.cc',
            'services/namespace_utils.h',
          ],
          'dependencies': [
            # for capability.h.
            'sandbox_services_headers',
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
        'system_headers/arm64_linux_syscalls.h',
        'system_headers/arm64_linux_ucontext.h',
        'system_headers/arm_linux_syscalls.h',
        'system_headers/arm_linux_ucontext.h',
        'system_headers/capability.h',
        'system_headers/i386_linux_ucontext.h',
        'system_headers/linux_futex.h',
        'system_headers/linux_seccomp.h',
        'system_headers/linux_syscalls.h',
        'system_headers/linux_time.h',
        'system_headers/linux_ucontext.h',
        'system_headers/mips_linux_syscalls.h',
        'system_headers/mips_linux_ucontext.h',
        'system_headers/x86_32_linux_syscalls.h',
        'system_headers/x86_64_linux_syscalls.h',
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
        'suid/client/setuid_sandbox_host.cc',
        'suid/client/setuid_sandbox_host.h',
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
    {
      'target_name': 'bpf_dsl_golden',
      'type': 'none',
      'actions': [
        {
          'action_name': 'generate',
          'inputs': [
            'bpf_dsl/golden/generate.py',
            'bpf_dsl/golden/i386/ArgSizePolicy.txt',
            'bpf_dsl/golden/i386/BasicPolicy.txt',
            'bpf_dsl/golden/i386/ElseIfPolicy.txt',
            'bpf_dsl/golden/i386/MaskingPolicy.txt',
            'bpf_dsl/golden/i386/MoreBooleanLogicPolicy.txt',
            'bpf_dsl/golden/i386/NegativeConstantsPolicy.txt',
            'bpf_dsl/golden/i386/SwitchPolicy.txt',
            'bpf_dsl/golden/x86-64/ArgSizePolicy.txt',
            'bpf_dsl/golden/x86-64/BasicPolicy.txt',
            'bpf_dsl/golden/x86-64/BooleanLogicPolicy.txt',
            'bpf_dsl/golden/x86-64/ElseIfPolicy.txt',
            'bpf_dsl/golden/x86-64/MaskingPolicy.txt',
            'bpf_dsl/golden/x86-64/MoreBooleanLogicPolicy.txt',
            'bpf_dsl/golden/x86-64/NegativeConstantsPolicy.txt',
            'bpf_dsl/golden/x86-64/SwitchPolicy.txt',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/sandbox/linux/bpf_dsl/golden/golden_files.h',
          ],
          'action': [
            'python',
            'linux/bpf_dsl/golden/generate.py',
            '<(SHARED_INTERMEDIATE_DIR)/sandbox/linux/bpf_dsl/golden/golden_files.h',
            'linux/bpf_dsl/golden/i386/ArgSizePolicy.txt',
            'linux/bpf_dsl/golden/i386/BasicPolicy.txt',
            'linux/bpf_dsl/golden/i386/ElseIfPolicy.txt',
            'linux/bpf_dsl/golden/i386/MaskingPolicy.txt',
            'linux/bpf_dsl/golden/i386/MoreBooleanLogicPolicy.txt',
            'linux/bpf_dsl/golden/i386/NegativeConstantsPolicy.txt',
            'linux/bpf_dsl/golden/i386/SwitchPolicy.txt',
            'linux/bpf_dsl/golden/x86-64/ArgSizePolicy.txt',
            'linux/bpf_dsl/golden/x86-64/BasicPolicy.txt',
            'linux/bpf_dsl/golden/x86-64/BooleanLogicPolicy.txt',
            'linux/bpf_dsl/golden/x86-64/ElseIfPolicy.txt',
            'linux/bpf_dsl/golden/x86-64/MaskingPolicy.txt',
            'linux/bpf_dsl/golden/x86-64/MoreBooleanLogicPolicy.txt',
            'linux/bpf_dsl/golden/x86-64/NegativeConstantsPolicy.txt',
            'linux/bpf_dsl/golden/x86-64/SwitchPolicy.txt',
          ],
          'message': 'Generating header from golden files ...',
        },
      ],
    },
  ],
  'conditions': [
    [ 'OS=="android"', {
      'targets': [
      {
        'target_name': 'sandbox_linux_unittests_deps',
        'type': 'none',
        'dependencies': [
          'sandbox_linux_unittests',
        ],
        'variables': {
           'output_dir': '<(PRODUCT_DIR)/sandbox_linux_unittests__dist/',
           'native_binary': '<(PRODUCT_DIR)/sandbox_linux_unittests',
           'include_main_binary': 1,
        },
        'includes': [
          '../../build/android/native_app_dependencies.gypi'
        ],
      }],
    }],
    [ 'OS=="android"', {
      'conditions': [
        ['test_isolation_mode != "noop"', {
          'targets': [
            {
              'target_name': 'sandbox_linux_unittests_apk_run',
              'type': 'none',
              'dependencies': [
                'sandbox_linux_unittests',
              ],
              'includes': [
                '../../build/isolate.gypi',
              ],
              'sources': [
                '../sandbox_linux_unittests_apk.isolate',
              ],
            },
          ],
        },
      ],
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
          ],
          'sources': [
            '../sandbox_linux_unittests.isolate',
          ],
        },
      ],
    }],
  ],
}
