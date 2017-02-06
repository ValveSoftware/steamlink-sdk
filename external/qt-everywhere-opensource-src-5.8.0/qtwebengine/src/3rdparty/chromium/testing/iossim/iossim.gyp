# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'mac_deployment_target': '10.9',
    'mac_sdk_min': '10.9',
    'class_dump_bin': '<(PRODUCT_DIR)/class-dump',
    'class_dump_py': '<(DEPTH)/third_party/class-dump/class-dump.py',
  },
  'targets': [
    {
      # GN version //testing/iossim(//build/toolchain/mac:clang_x64)
      'target_name': 'iossim',
      'toolsets': ['host'],
      'type': 'executable',
      'variables': {
        'developer_dir': '<!(xcode-select -print-path)',
        'iphone_sim_path': '<(developer_dir)/../SharedFrameworks',
      },
      'dependencies': [
        '<(DEPTH)/third_party/class-dump/class-dump.gyp:class-dump#host',
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)/iossim',
      ],
      'sources': [
        'iossim.mm',
        '<(INTERMEDIATE_DIR)/iossim/iPhoneSimulatorRemoteClient.h',
      ],
      'libraries': [
        '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
      ],
      'actions': [
        {
          'action_name': 'generate_dvt_core_simulator',
          'inputs': [
            '<(class_dump_bin)',
            '<(class_dump_py)',
            '<(developer_dir)/Library/PrivateFrameworks/CoreSimulator.framework/Versions/Current/CoreSimulator',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/iossim/CoreSimulator.h'
          ],
          'action': [
            'python',
            '<(class_dump_py)',
            '-t', '<(class_dump_bin)',
            '-o', '<(INTERMEDIATE_DIR)/iossim/CoreSimulator.h',
            '--',
            '-CSim',
            '<(developer_dir)/Library/PrivateFrameworks/CoreSimulator.framework',
          ],
          'message': 'Generating CoreSimulator.h',
        },
        {
          'action_name': 'generate_dvt_foundation_header',
          'inputs': [
            '<(class_dump_bin)',
            '<(class_dump_py)',
            '<(iphone_sim_path)/DVTFoundation.framework/Versions/Current/DVTFoundation',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/iossim/DVTFoundation.h'
          ],
          'action': [
            'python',
            '<(class_dump_py)',
            '-t', '<(class_dump_bin)',
            '-o', '<(INTERMEDIATE_DIR)/iossim/DVTFoundation.h',
            '--',
            '-CDVTStackBacktrace|DVTInvalidation|DVTMixIn',
            '<(iphone_sim_path)/DVTFoundation.framework',
          ],
          'message': 'Generating DVTFoundation.h',
        },
        {
          'action_name': 'generate_dvt_iphone_sim_header',
          'inputs': [
            '<(class_dump_bin)',
            '<(class_dump_py)',
            '<(iphone_sim_path)/DVTiPhoneSimulatorRemoteClient.framework/Versions/Current/DVTiPhoneSimulatorRemoteClient',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/iossim/DVTiPhoneSimulatorRemoteClient.h'
          ],
          'action': [
            'python',
            '<(class_dump_py)',
            '-t', '<(class_dump_bin)',
            '-o', '<(INTERMEDIATE_DIR)/iossim/DVTiPhoneSimulatorRemoteClient.h',
            '--',
            '-I',
            '-CiPhoneSimulator',
            '<(iphone_sim_path)/DVTiPhoneSimulatorRemoteClient.framework',
          ],
          'message': 'Generating DVTiPhoneSimulatorRemoteClient.h',
        },
      ],  # actions
      'xcode_settings': {
        'ARCHS': ['x86_64'],
      },
    },
  ],
}
