# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{

  'includes': [
    'gles2_conform.gypi',
  ],
  'target_defaults': {
    'msvs_disabled_warnings': [4005, 4013, 4018, 4101, 4716],
     'include_dirs': [
       '<(SHARED_INTERMEDIATE_DIR)/gles2_conform_test_embedded_data',
       '../../third_party/gles2_conform/GTF_ES/glsl/GTF/Source',
     ],
     'variables': {
       'clang_warning_flags': [
         '-Wno-array-bounds',
         # There are some implicit conversions from "int" to "char" in
         # GTFExtensionTestSurfacelessContext.c.
         '-Wno-constant-conversion',
         '-Wno-implicit-function-declaration',
         '-Wno-incompatible-pointer-types',
         '-Wno-logical-op-parentheses',
         # Many struct initializers in the GTF_ES code are missing braces.
         '-Wno-missing-braces',
         '-Wno-parentheses-equality',
         '-Wno-pointer-sign',
         '-Wno-return-type',
         '-Wno-sizeof-pointer-memaccess',
         # A few variables are unitialized if GLVersion != 2.0.
         '-Wno-sometimes-uninitialized',
         '-Wno-tautological-compare',
         # GTFVecBase.h contains static no-inline functions in a header :-/
         '-Wno-unused-function',
       ],
     },
  },
  'targets': [
    {
      'target_name': 'gles2_conform_test',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/gpu/gpu.gyp:gpu',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'sources': [
        'gles2_conform_test.cc',
      ],
      'conditions': [
        ['internal_gles2_conform_tests', {
          'dependencies': [
            'gles2_conform_test_windowless',
          ],
          'variables': {
            'gles2_conform_test_output_dir': '<(SHARED_INTERMEDIATE_DIR)/gpu/gles2_conform_test',
          },
          'sources': [
            '<(gles2_conform_test_output_dir)/gles2_conform_test_autogen.cc',
          ],
          'actions': [
            {
              'action_name': 'generate_gles2_conform_tests',
              'inputs': [
                'generate_gles2_conform_tests.py',
                'gles2_conform_test.h',
                '<(DEPTH)/third_party/gles2_conform/GTF_ES/glsl/GTF/mustpass_es20.run',
              ],
              'outputs': [
                '<(gles2_conform_test_output_dir)/gles2_conform_test_autogen.cc',
              ],
              'action': [
                'python',
                'generate_gles2_conform_tests.py',
                '<(gles2_conform_test_output_dir)',
              ],
            },
          ],
        }], # internal_gles2_conform_tests
      ],
    },
    # TODO(alokp): Add gles2_conform_test_pepper target
  ],
  'conditions': [
    ['internal_gles2_conform_tests', {
      'targets': [
        {
          'target_name': 'gles2_conform_test_embedded_data',
          'type': 'none',
          'hard_dependency': 1,
          'actions': [
            {
              'action_name': 'generate_gles2_conform_embedded_files',
              'variables': {
                'generator_path': 'generate_gles2_embedded_data.py',
              },
              'inputs': [
                '<(generator_path)',
                '<!@(python <(generator_path) ../../third_party/gles2_conform/GTF_ES/glsl/GTF)',
              ],
              'outputs': [
                #'../../third_party/gles2_conform/GTF_ES/glsl/GTF/Source/FilesDATA.c',
                #'../../third_party/gles2_conform/GTF_ES/glsl/GTF/Source/FilesDATA.h',
                #'../../third_party/gles2_conform/GTF_ES/glsl/GTF/Source/FilesTOC.c',
                '<(SHARED_INTERMEDIATE_DIR)/gles2_conform_test_embedded_data/FilesDATA.c',
                '<(SHARED_INTERMEDIATE_DIR)/gles2_conform_test_embedded_data/FilesDATA.h',
                '<(SHARED_INTERMEDIATE_DIR)/gles2_conform_test_embedded_data/FilesTOC.c',
              ],
              'action': [
                'python',
                '<(generator_path)',
                '../../third_party/gles2_conform/GTF_ES/glsl/GTF',
                '<(SHARED_INTERMEDIATE_DIR)/gles2_conform_test_embedded_data',
              ],
            },
          ],
        },
        {
          'target_name': 'gles2_conform_test_windowless',
          'type': 'executable',
          'dependencies': [
            'gles2_conform_test_embedded_data',
            '<(DEPTH)/gpu/gles2_conform_support/gles2_conform_support.gyp:egl_native',
            '<(DEPTH)/gpu/gles2_conform_support/gles2_conform_support.gyp:egl_main_windowless',
            '<(DEPTH)/gpu/gpu.gyp:gles2_c_lib_nocheck',
            '<(DEPTH)/third_party/expat/expat.gyp:expat',
          ],
          'conditions': [
            ['OS=="linux"', {
              'conditions': [
                [ 'chromeos==0', {
                  'dependencies': ['../../build/linux/system.gyp:gtk2']
                }],
              ],
            }],
            ['OS=="win"', {
              'dependencies': [
                '<(angle_path)/src/angle.gyp:libGLESv2',
                '<(angle_path)/src/angle.gyp:libEGL',
              ],
              'defines': [
                'EGLAPI=',
                'EGLAPIENTRY=',
              ],
              'msvs_disabled_warnings': [
                4018,  # signed/unsigned mismatch
                4101,  # unreferenced local variable
                4715,  # not all control paths return a value
                4267,  # size_t/unsigned int conversion
              ],
              'defines!': [ 'NOMINMAX' ],
            }],
            ['OS=="mac"', {
              'defines': [
                '_STDINT',
                '_STDINT_H',
              ],
              'conditions': [
                [ 'clang==1', {
                  'xcode_settings': {
                    'LD': 'clang++',
                    'WARNING_CFLAGS': [
                      '-Wno-pointer-sign',
                      '-Wno-array-bounds',
                      '-Wno-sizeof-pointer-memaccess',
                      '-Wno-implicit-function-declaration',
                      '-Wno-logical-op-parentheses',
                      '-Wno-tautological-compare',
                      '-Wno-parentheses-equality',
                      '-Wno-return-type',
                    ],
                  },
                }],
              ],
            }],
          ],
          'defines': [
            'GTF_API=GTF_GLES20',
            'HKEMBEDDEDFILESYSTEM',
          ],
          'sources': [
            '<@(gtf_es_sources)',
            '<@(gl2_extension_test_sources)',
            '<@(gl2_fixed_test_sources)',
            '<@(gl2_test_sources)',
            # Include a dummy c++ file to force linking of libstdc++.
            '<(DEPTH)/gpu/gles2_conform_support/dummy.cc',
          ],
          'run_as': {
            'conditions': [
              ['OS=="win"', {
                'action': [
                  '$(TargetPath)',
                  '-noimagefileio',
                  '-run=<(DEPTH)/third_party/gles2_conform/GTF_ES/glsl/GTF/mustpass.run',
                ],
              }],
            ],
          },
        },
      ],
    }], # internal_gles2_conform_tests
    ['OS=="win" and internal_gles2_conform_tests', {
      'targets': [
        {
          'target_name': 'gles2_conform_test_angle',
          'type': 'executable',
          'dependencies': [
            'gles2_conform_test_embedded_data',
            '<(DEPTH)/base/base.gyp:base',
            '<(DEPTH)/third_party/expat/expat.gyp:expat',
            '<(angle_path)/src/angle.gyp:libGLESv2',
            '<(angle_path)/src/angle.gyp:libEGL',
            '<(DEPTH)/gpu/gles2_conform_support/gles2_conform_support.gyp:egl_main_native',
          ],
          'defines': [
            'GTF_API=GTF_GLES20',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/angle_dx11/include',
          ],
          'sources': [
            '<@(gtf_es_sources)',
            '<@(gl2_extension_test_sources)',
            '<@(gl2_fixed_test_sources)',
            '<@(gl2_test_sources)',
          ],
        },
      ],
    }],

    ['chromeos==1 and internal_gles2_conform_tests', {
      'targets': [
        {
          'target_name': 'gles2_conform_test_chromeos',
          'type': 'executable',
          'dependencies': [
            'gles2_conform_test_embedded_data',
            '<(DEPTH)/base/base.gyp:*',
            '<(DEPTH)/third_party/expat/expat.gyp:expat',
            '<(DEPTH)/gpu/gles2_conform_support/gles2_conform_support.gyp:egl_main_native',
          ],
          'libraries': [
            '-lEGL',
            '-lGLESv2',
          ],
          'defines': [
            'GTF_API=GTF_GLES20',
            'CHROMEOS_GLES2_CONFORMANCE',
          ],
          'sources': [
            '<@(gtf_es_sources)',
            '<@(gl2_extension_test_sources)',
            '<@(gl2_fixed_test_sources)',
            '<@(gl2_test_sources)',
          ],
        },
      ],
    }],
  ],
}


# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
