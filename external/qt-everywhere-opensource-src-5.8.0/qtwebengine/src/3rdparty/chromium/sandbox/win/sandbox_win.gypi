# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'sandbox_windows_target': 0,
      'target_arch%': 'ia32',
    },
    'target_conditions': [
      ['sandbox_windows_target==1', {
        # Files that are shared between the 32-bit and the 64-bit versions
        # of the Windows sandbox library.
        'sources': [
            'src/acl.cc',
            'src/acl.h',
            'src/broker_services.cc',
            'src/broker_services.h',
            'src/crosscall_client.h',
            'src/crosscall_params.h',
            'src/crosscall_server.cc',
            'src/crosscall_server.h',
            'src/eat_resolver.cc',
            'src/eat_resolver.h',
            'src/filesystem_dispatcher.cc',
            'src/filesystem_dispatcher.h',
            'src/filesystem_interception.cc',
            'src/filesystem_interception.h',
            'src/filesystem_policy.cc',
            'src/filesystem_policy.h',
            'src/handle_closer.cc',
            'src/handle_closer.h',
            'src/handle_closer_agent.cc',
            'src/handle_closer_agent.h',
            'src/interception.cc',
            'src/interception.h',
            'src/interception_agent.cc',
            'src/interception_agent.h',
            'src/interception_internal.h',
            'src/interceptors.h',
            'src/internal_types.h',
            'src/ipc_tags.h',
            'src/job.cc',
            'src/job.h',
            'src/named_pipe_dispatcher.cc',
            'src/named_pipe_dispatcher.h',
            'src/named_pipe_interception.cc',
            'src/named_pipe_interception.h',
            'src/named_pipe_policy.cc',
            'src/named_pipe_policy.h',
            'src/nt_internals.h',
            'src/policy_broker.cc',
            'src/policy_broker.h',
            'src/policy_engine_opcodes.cc',
            'src/policy_engine_opcodes.h',
            'src/policy_engine_params.h',
            'src/policy_engine_processor.cc',
            'src/policy_engine_processor.h',
            'src/policy_low_level.cc',
            'src/policy_low_level.h',
            'src/policy_params.h',
            'src/policy_target.cc',
            'src/policy_target.h',
            'src/process_mitigations.cc',
            'src/process_mitigations.h',
            'src/process_mitigations_win32k_dispatcher.cc',
            'src/process_mitigations_win32k_dispatcher.h',
            'src/process_mitigations_win32k_interception.cc',
            'src/process_mitigations_win32k_interception.h',
            'src/process_mitigations_win32k_policy.cc',
            'src/process_mitigations_win32k_policy.h',
            'src/process_thread_dispatcher.cc',
            'src/process_thread_dispatcher.h',
            'src/process_thread_interception.cc',
            'src/process_thread_interception.h',
            'src/process_thread_policy.cc',
            'src/process_thread_policy.h',
            'src/registry_dispatcher.cc',
            'src/registry_dispatcher.h',
            'src/registry_interception.cc',
            'src/registry_interception.h',
            'src/registry_policy.cc',
            'src/registry_policy.h',
            'src/resolver.cc',
            'src/resolver.h',
            'src/restricted_token_utils.cc',
            'src/restricted_token_utils.h',
            'src/restricted_token.cc',
            'src/restricted_token.h',
            'src/sandbox_factory.h',
            'src/sandbox_globals.cc',
            'src/sandbox_nt_types.h',
            'src/sandbox_nt_util.cc',
            'src/sandbox_nt_util.h',
            'src/sandbox_policy_base.cc',
            'src/sandbox_policy_base.h',
            'src/sandbox_policy.h',
            'src/sandbox_rand.cc',
            'src/sandbox_rand.h',
            'src/sandbox_types.h',
            'src/sandbox_utils.cc',
            'src/sandbox_utils.h',
            'src/sandbox.cc',
            'src/sandbox.h',
            'src/security_level.h',
            'src/service_resolver.cc',
            'src/service_resolver.h',
            'src/sharedmem_ipc_client.cc',
            'src/sharedmem_ipc_client.h',
            'src/sharedmem_ipc_server.cc',
            'src/sharedmem_ipc_server.h',
            'src/sid.cc',
            'src/sid.h',
            'src/sync_dispatcher.cc',
            'src/sync_dispatcher.h',
            'src/sync_interception.cc',
            'src/sync_interception.h',
            'src/sync_policy.cc',
            'src/sync_policy.h',
            'src/target_interceptions.cc',
            'src/target_interceptions.h',
            'src/target_process.cc',
            'src/target_process.h',
            'src/target_services.cc',
            'src/target_services.h',
            'src/top_level_dispatcher.cc',
            'src/top_level_dispatcher.h',
            'src/win_utils.cc',
            'src/win_utils.h',
            'src/win2k_threadpool.cc',
            'src/win2k_threadpool.h',
            'src/window.cc',
            'src/window.h',
        ],
        'target_conditions': [
          ['target_arch=="x64"', {
            'sources': [
              'src/interceptors_64.cc',
              'src/interceptors_64.h',
              'src/resolver_64.cc',
              'src/service_resolver_64.cc',
            ],
          }],
          ['target_arch=="ia32"', {
            'sources': [
              'src/resolver_32.cc',
              'src/service_resolver_32.cc',
              'src/sidestep_resolver.cc',
              'src/sidestep_resolver.h',
              'src/sidestep\ia32_modrm_map.cpp',
              'src/sidestep\ia32_opcode_map.cpp',
              'src/sidestep\mini_disassembler_types.h',
              'src/sidestep\mini_disassembler.cpp',
              'src/sidestep\mini_disassembler.h',
              'src/sidestep\preamble_patcher_with_stub.cpp',
              'src/sidestep\preamble_patcher.h',
            ],
          }],
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'sandbox',
      'type': 'static_library',
      'variables': {
        'sandbox_windows_target': 1,
      },
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_static',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '../..',
      ],
      'target_conditions': [
        ['target_arch=="ia32"', {
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)',
              'files': [
                'wow_helper/wow_helper.exe',
                'wow_helper/wow_helper.pdb',
              ],
            },
          ],
        }],
      ],
    },
    {
      'target_name': 'sbox_integration_tests',
      'type': 'executable',
      'dependencies': [
        'sandbox',
        'sbox_integration_test_hook_dll',
        'sbox_integration_test_win_proc',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'src/address_sanitizer_test.cc',
        'src/app_container_test.cc',
        'src/file_policy_test.cc',
        'src/handle_inheritance_test.cc',
        'tests/integration_tests/integration_tests_test.cc',
        'src/handle_closer_test.cc',
        'src/integrity_level_test.cc',
        'src/ipc_ping_test.cc',
        'src/lpc_policy_test.cc',
        'src/named_pipe_policy_test.cc',
        'src/policy_target_test.cc',
        'src/process_mitigations_test.cc',
        'src/process_policy_test.cc',
        'src/registry_policy_test.cc',
        'src/restricted_token_test.cc',
        'src/sync_policy_test.cc',
        'src/sync_policy_test.h',
        'src/unload_dll_test.cc',
        'tests/common/controller.cc',
        'tests/common/controller.h',
        'tests/common/test_utils.cc',
        'tests/common/test_utils.h',
        'tests/integration_tests/integration_tests.cc',
        'tests/integration_tests/integration_tests_common.h',
      ],
      'link_settings': {
        'libraries': [
          '-ldxva2.lib',
        ],
      },
    },
    {
      'target_name': 'sbox_integration_test_hook_dll',
      'type': 'shared_library',
      'dependencies': [
      ],
      'sources': [
        'tests/integration_tests/hooking_dll.cc',
        'tests/integration_tests/integration_tests_common.h',
      ],
    },
    {
      'target_name': 'sbox_integration_test_win_proc',
      'type': 'executable',
      'dependencies': [
      ],
      'sources': [
        'tests/integration_tests/hooking_win_proc.cc',
        'tests/integration_tests/integration_tests_common.h',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '2',  # Set /SUBSYSTEM:WINDOWS
        },
      },
    },
    {
      'target_name': 'sbox_validation_tests',
      'type': 'executable',
      'dependencies': [
        'sandbox',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'tests/common/controller.cc',
        'tests/common/controller.h',
        'tests/validation_tests/unit_tests.cc',
        'tests/validation_tests/commands.cc',
        'tests/validation_tests/commands.h',
        'tests/validation_tests/suite.cc',
      ],
      'link_settings': {
        'libraries': [
          '-lshlwapi.lib',
        ],
      },
    },
    {
      'target_name': 'sbox_unittests',
      'type': 'executable',
      'dependencies': [
        'sandbox',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'src/interception_unittest.cc',
        'src/service_resolver_unittest.cc',
        'src/restricted_token_unittest.cc',
        'src/job_unittest.cc',
        'src/sid_unittest.cc',
        'src/policy_engine_unittest.cc',
        'src/policy_low_level_unittest.cc',
        'src/policy_opcodes_unittest.cc',
        'src/ipc_unittest.cc',
        'src/sandbox_nt_util_unittest.cc',
        'src/threadpool_unittest.cc',
        'src/win_utils_unittest.cc',
        'tests/common/test_utils.cc',
        'tests/common/test_utils.h',
        'tests/unit_tests/unit_tests.cc',
      ],
    },
    {
      'target_name': 'sandbox_poc',
      'type': 'executable',
      'dependencies': [
        'sandbox',
        'pocdll',
      ],
      'sources': [
        'sandbox_poc/main_ui_window.cc',
        'sandbox_poc/main_ui_window.h',
        'sandbox_poc/resource.h',
        'sandbox_poc/sandbox.cc',
        'sandbox_poc/sandbox.h',
        'sandbox_poc/sandbox.ico',
        'sandbox_poc/sandbox.rc',
      ],
      'link_settings': {
        'libraries': [
          '-lcomctl32.lib',
        ],
      },
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '2',         # Set /SUBSYSTEM:WINDOWS
        },
      },
    },
    {
      'target_name': 'pocdll',
      'type': 'shared_library',
      'sources': [
        'sandbox_poc/pocdll/exports.h',
        'sandbox_poc/pocdll/fs.cc',
        'sandbox_poc/pocdll/handles.cc',
        'sandbox_poc/pocdll/invasive.cc',
        'sandbox_poc/pocdll/network.cc',
        'sandbox_poc/pocdll/pocdll.cc',
        'sandbox_poc/pocdll/processes_and_threads.cc',
        'sandbox_poc/pocdll/registry.cc',
        'sandbox_poc/pocdll/spyware.cc',
        'sandbox_poc/pocdll/utils.h',
      ],
      'defines': [
        'POCDLL_EXPORTS',
      ],
      'include_dirs': [
        '../..',
      ],
    },
  ],
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'sandbox_win64',
          'type': 'static_library',
          'variables': {
            'sandbox_windows_target': 1,
            'target_arch': 'x64',
          },
          'dependencies': [
            '../base/base.gyp:base_win64',
            '../base/base.gyp:base_static_win64',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
          'include_dirs': [
            '../..',
          ],
          'defines': [
            '<@(nacl_win64_defines)',
          ]
        },
      ],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'sbox_integration_tests_run',
          'type': 'none',
          'dependencies': [
            'sbox_integration_tests',
          ],
          'includes': [
            '../../build/isolate.gypi',
          ],
          'sources': [
            '../sbox_integration_tests.isolate',
          ],
        },
        {
          'target_name': 'sbox_unittests_run',
          'type': 'none',
          'dependencies': [
            'sbox_unittests',
          ],
          'includes': [
            '../../build/isolate.gypi',
          ],
          'sources': [
            '../sbox_unittests.isolate',
          ],
        },
        {
          'target_name': 'sbox_validation_tests_run',
          'type': 'none',
          'dependencies': [
            'sbox_validation_tests',
          ],
          'includes': [
            '../../build/isolate.gypi',
          ],
          'sources': [
            '../sbox_validation_tests.isolate',
          ],
        },
      ],
    }],
  ],
}
