# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'courgette_lib_sources': [
      'adjustment_method.cc',
      'adjustment_method_2.cc',
      'adjustment_method.h',
      'assembly_program.cc',
      'assembly_program.h',
      'consecutive_range_visitor.h',
      'courgette.h',
      'crc.cc',
      'crc.h',
      'difference_estimator.cc',
      'difference_estimator.h',
      'disassembler.cc',
      'disassembler.h',
      'disassembler_elf_32.cc',
      'disassembler_elf_32.h',
      'disassembler_elf_32_arm.cc',
      'disassembler_elf_32_arm.h',
      'disassembler_elf_32_x86.cc',
      'disassembler_elf_32_x86.h',
      'disassembler_win32.cc',
      'disassembler_win32.h',
      'disassembler_win32_x86.cc',
      'disassembler_win32_x86.h',
      'disassembler_win32_x64.cc',
      'disassembler_win32_x64.h',
      'encoded_program.cc',
      'encoded_program.h',
      'ensemble.cc',
      'ensemble.h',
      'ensemble_apply.cc',
      'ensemble_create.cc',
      'image_utils.h',
      'label_manager.cc',
      'label_manager.h',
      'memory_allocator.cc',
      'memory_allocator.h',
      'patch_generator_x86_32.h',
      'patcher_x86_32.h',
      'program_detector.cc',
      'program_detector.h',
      'region.h',
      'rel32_finder.cc',
      'rel32_finder.h',
      'rel32_finder_x64.cc',
      'rel32_finder_x64.h',
      'rel32_finder_x86.cc',
      'rel32_finder_x86.h',
      'simple_delta.cc',
      'simple_delta.h',
      'streams.cc',
      'streams.h',
      'third_party/bsdiff/bsdiff.h',
      'third_party/bsdiff/bsdiff_apply.cc',
      'third_party/bsdiff/bsdiff_create.cc',
      'third_party/bsdiff/bsdiff_search.h',
      'third_party/bsdiff/paged_array.h',
      'third_party/bsdiff/qsufsort.h',
      'types_elf.h',
      'types_win_pe.h',
    ],
  },
  'targets': [
    {
      'target_name': 'courgette_lib',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/lzma_sdk/lzma_sdk.gyp:lzma_sdk',
      ],
      'sources': [
        '<@(courgette_lib_sources)'
      ],
    },
    {
      'target_name': 'courgette',
      'type': 'executable',
      'sources': [
        'courgette_tool.cc',
      ],
      'dependencies': [
        'courgette_lib',
        '../base/base.gyp:base',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'LargeAddressAware': 2,
        },
      },
    },
    {
      'target_name': 'courgette_minimal_tool',
      'type': 'executable',
      'sources': [
        'courgette_minimal_tool.cc',
      ],
      'dependencies': [
        'courgette_lib',
        '../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'courgette_unittests',
      'type': 'executable',
      'sources': [
        'adjustment_method_unittest.cc',
        'bsdiff_memory_unittest.cc',
        'base_test_unittest.cc',
        'base_test_unittest.h',
        'consecutive_range_visitor_unittest.cc',
        'difference_estimator_unittest.cc',
        'disassembler_elf_32_x86_unittest.cc',
        'disassembler_win32_x86_unittest.cc',
        'disassembler_win32_x64_unittest.cc',
        'encoded_program_unittest.cc',
        'encode_decode_unittest.cc',
        'ensemble_unittest.cc',
        'image_utils_unittest.cc',
        'label_manager_unittest.cc',
        'memory_allocator_unittest.cc',
        'rel32_finder_unittest.cc',
        'streams_unittest.cc',
        'typedrva_unittest.cc',
        'versioning_unittest.cc',
        'third_party/bsdiff/bsdiff_search_unittest.cc',
        'third_party/bsdiff/paged_array_unittest.cc',
        'third_party/bsdiff/qsufsort_unittest.cc',
      ],
      'dependencies': [
        'courgette_lib',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:run_all_unittests',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
    {
      'target_name': 'courgette_fuzz',
      'type': 'executable',
      'sources': [
        'base_test_unittest.cc',
        'base_test_unittest.h',
        'encoded_program_fuzz_unittest.cc',
       ],
      'dependencies': [
        'courgette_lib',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
      ],
    },
  ],
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'courgette_lib64',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base_win64',
            '../third_party/lzma_sdk/lzma_sdk.gyp:lzma_sdk64',
          ],
          'sources': [
            '<@(courgette_lib_sources)',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
        {
          'target_name': 'courgette64',
          'type': 'executable',
          'sources': [
            'courgette_tool.cc',
          ],
          'dependencies': [
            'courgette_lib64',
            '../base/base.gyp:base_win64',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
    # The build infrastructure needs courgette to be named courgette64.
    ['OS=="win" and target_arch=="x64"', {
      'targets': [
        {
          'target_name': 'courgette64',
          'type': 'none',
          'dependencies': [
            'courgette',
          ],
          'actions': [{
            'action_name': 'courgette64',
            'inputs': [
              '<(PRODUCT_DIR)/courgette.exe',
            ],
            'outputs': [
              '<(PRODUCT_DIR)/courgette64.exe',
            ],
            'action': [
              'python',
              '../build/cp.py',
              '<@(_inputs)',
              '<@(_outputs)'
            ],
          }],
        },
      ],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'courgette_unittests_run',
          'type': 'none',
          'dependencies': [
            'courgette_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'courgette_unittests.isolate',
          ],
        },
      ],
    }],
  ],
}
