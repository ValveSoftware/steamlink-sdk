# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
  {
    'target_name': 'shell_lib',
    'type': 'static_library',
    'sources': [
      '../catalog/catalog.cc',
      '../catalog/catalog.h',
      '../catalog/constants.cc',
      '../catalog/constants.h',
      '../catalog/entry.cc',
      '../catalog/entry.h',
      '../catalog/instance.cc',
      '../catalog/instance.h',
      '../catalog/reader.cc',
      '../catalog/reader.h',
      '../catalog/store.cc',
      '../catalog/store.h',
      '../catalog/types.h',
      'connect_params.cc',
      'connect_params.h',
      'connect_util.cc',
      'connect_util.h',
      'native_runner.h',
      'native_runner_delegate.h',
      'shell.cc',
      'shell.h',
      'switches.cc',
      'switches.cc',
    ],
    'dependencies': [
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      '<(DEPTH)/components/filesystem/filesystem.gyp:filesystem_bindings',
      '<(DEPTH)/components/filesystem/filesystem.gyp:filesystem_lib',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_common_lib',
      'shell_public.gyp:shell_public',
    ],
    'export_dependent_settings': [
      '<(DEPTH)/components/filesystem/filesystem.gyp:filesystem_bindings',
      'shell_public.gyp:shell_public',
    ],
    'variables': {
      'mojom_typemaps': [
        '<(DEPTH)/mojo/common/common_custom_types.typemap',
      ],
    }
  }, {
    'target_name': 'mojo_shell_unittests',
    'type': 'executable',
    'sources': [
      'tests/placeholder_unittest.cc',
    ],
    'dependencies': [
      'shell_lib',
      'shell_test_public',
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_common_lib',
      '<(DEPTH)/mojo/mojo_edk.gyp:mojo_run_all_unittests',
      '<(DEPTH)/mojo/mojo_public.gyp:mojo_cpp_bindings',
      '<(DEPTH)/testing/gtest.gyp:gtest',
      'shell_public.gyp:shell_public',
    ]
  }, {
    'target_name': 'shell_test_public',
    'type': 'static_library',
    'dependencies': [
      'shell_test_interfaces',
    ],
  }, {
    'target_name': 'shell_test_interfaces',
    'type': 'none',
    'variables': {
      'mojom_files': [
        'tests/test.mojom',
      ],
    },
    'includes': [
      '../../mojo/mojom_bindings_generator_explicit.gypi',
    ],
  }, {
    'target_name': 'shell_runner_common_lib',
    'type': 'static_library',
    'sources': [
      'runner/common/client_util.cc',
      'runner/common/client_util.h',
      'runner/common/switches.cc',
      'runner/common/switches.h',
    ],
    'include_dirs': [
      '..',
    ],
    'dependencies': [
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/mojo/mojo_edk.gyp:mojo_system_impl',
      '<(DEPTH)/mojo/mojo_public.gyp:mojo_cpp_bindings',
      '<(DEPTH)/mojo/mojo_public.gyp:mojo_cpp_system',
      'shell_public.gyp:shell_public',
    ],
    'export_dependent_settings': [
      'shell_public.gyp:shell_public',
    ],
  }, {
    'target_name': 'shell_runner_host_lib',
    'type': 'static_library',
    'sources': [
      'runner/host/child_process.cc',
      'runner/host/child_process.h',
      'runner/host/child_process_base.cc',
      'runner/host/child_process_base.h',
      'runner/host/child_process_host.cc',
      'runner/host/child_process_host.h',
      'runner/host/in_process_native_runner.cc',
      'runner/host/in_process_native_runner.h',
      'runner/host/native_application_support.cc',
      'runner/host/native_application_support.h',
      'runner/host/out_of_process_native_runner.cc',
      'runner/host/out_of_process_native_runner.h',
      'runner/init.cc',
      'runner/init.h',
    ],
    'dependencies': [
      'shell_lib',
      'shell_runner_common_lib',
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/base/base.gyp:base_i18n',
      '<(DEPTH)/base/base.gyp:base_static',
      '<(DEPTH)/mojo/mojo_edk.gyp:mojo_system_impl',
      'shell_public.gyp:shell_public',
    ],
    'export_dependent_settings': [
      'shell_public.gyp:shell_public',
    ],
    'conditions': [
      ['OS=="linux"', {
        'sources': [
          'runner/host/linux_sandbox.cc',
          'runner/host/linux_sandbox.h',
        ],
        'dependencies': [
          '<(DEPTH)/sandbox/sandbox.gyp:sandbox',
          '<(DEPTH)/sandbox/sandbox.gyp:sandbox_services',
          '<(DEPTH)/sandbox/sandbox.gyp:seccomp_bpf',
          '<(DEPTH)/sandbox/sandbox.gyp:seccomp_bpf_helpers',
        ],
      }],
      ['OS=="mac"', {
        'sources': [
          'runner/host/mach_broker.cc',
          'runner/host/mach_broker.h',
        ],
      }],
      ['icu_use_data_file_flag==1', {
        'defines': ['ICU_UTIL_DATA_IMPL=ICU_UTIL_DATA_FILE'],
      }, { # else icu_use_data_file_flag !=1
        'conditions': [
          ['OS=="win"', {
            'defines': ['ICU_UTIL_DATA_IMPL=ICU_UTIL_DATA_SHARED'],
          }, {
            'defines': ['ICU_UTIL_DATA_IMPL=ICU_UTIL_DATA_STATIC'],
          }],
        ],
      }],
    ],
  }, {
    # GN version: //services/catalog:manifest
    'target_name': 'catalog_manifest',
    'type': 'none',
    'variables': {
      'application_type': 'mojo',
      'application_name': 'catalog',
      'source_manifest': '<(DEPTH)/services/catalog/manifest.json',
    },
    'includes': [
      '../../mojo/public/mojo_application_manifest.gypi',
    ],
    'hard_dependency': 1,
  }, {
    # GN version: //services/shell/public/cpp/tests
    'target_name': 'shell_client_lib_unittests',
    'type': 'executable',
    'dependencies': [
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/mojo/mojo_edk.gyp:mojo_run_all_unittests',
      '<(DEPTH)/testing/gtest.gyp:gtest',
      'shell_public.gyp:shell_public',
    ],
    'sources': [
      'public/cpp/tests/interface_registry_unittest.cc',
    ],
  }],
  'conditions': [
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'mojo_shell_unittests_run',
          'type': 'none',
          'dependencies': [
            'mojo_shell_unittests',
          ],
          'includes': [
            '../../build/isolate.gypi',
          ],
          'sources': [
            'mojo_shell_unittests.isolate',
          ],
        },
      ],
    }],
  ],
}
