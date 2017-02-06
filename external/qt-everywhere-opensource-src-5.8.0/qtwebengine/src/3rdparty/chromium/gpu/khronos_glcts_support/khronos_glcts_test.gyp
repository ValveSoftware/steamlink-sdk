# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'khronos_glcts.gypi',
    'khronos_glcts_defaults.gypi',
  ],
  'targets': [
    {
      'target_name': 'khronos_glcts_test',
      'type': 'executable',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/gpu/gpu.gyp:gpu',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'sources': [
        'khronos_glcts_test.cc',
      ],
      'conditions': [
        ['internal_khronos_glcts_tests', {
          'dependencies': [
            'khronos_glcts_test_windowless',
          ],
          'variables': {
            'khronos_glcts_test_output_dir': '<(SHARED_INTERMEDIATE_DIR)/gpu/khronos_glcts_test',
          },
          'sources': [
            '<(khronos_glcts_test_output_dir)/khronos_glcts_test_autogen.cc',
          ],
          'actions': [
            {
              'action_name': 'generate_khronos_glcts_tests',
              'inputs': [
                'generate_khronos_glcts_tests.py',
                'khronos_glcts_test.h',
                '<@(glcts_gtf_runfiles)',
              ],
              'outputs': [
                '<(khronos_glcts_test_output_dir)/khronos_glcts_test_autogen.cc',
              ],
              'action': [
                'python',
                'generate_khronos_glcts_tests.py',
                '--outdir=<(khronos_glcts_test_output_dir)',
                '<@(glcts_gtf_runfiles)',
              ],
            },
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['internal_khronos_glcts_tests', {
      'targets': [
        {
          'target_name': 'tcutil_platform_windowless',
          'type': 'static_library',
          'dependencies': [
            '<(DEPTH)/gpu/khronos_glcts_support/khronos_glcts_framework.gyp:khronos_glcts_framework',
          ],
          'sources': [
            'native/egl_native_windowless.cc',
          ],
        },
        {
          'target_name': 'khronos_glcts_test_windowless',
          'type': 'executable',
          'dependencies': [
            'tcutil_platform_windowless',
            '<(DEPTH)/gpu/khronos_glcts_support/khronos_glcts_framework.gyp:khronos_glcts_framework',
            '<(DEPTH)/gpu/khronos_glcts_support/khronos_glcts_cts.gyp:glcts_es2',
          ],
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/khronos_glcts_data/gl_cts',
              'files': [
                '<@(glcts_data_dirs)',
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/khronos_glcts_data/gl_cts/GTF',
              'files': [
                '<@(glcts_gtf_data_dirs)',
                '<@(glcts_gtf_runfiles)',
              ],
            },
          ],
          'sources': [
            '<(DEPTH)/third_party/khronos_glcts/cts/glcTestPackageEntry.cpp',
            '<(DEPTH)/third_party/khronos_glcts/cts/glcTestPackageRegistry.cpp',
            '<(DEPTH)/third_party/khronos_glcts/cts/glcTestPackageRegistry.hpp',
            'native/main.cc',
          ],
        },
      ],
    }],
  ],
}
