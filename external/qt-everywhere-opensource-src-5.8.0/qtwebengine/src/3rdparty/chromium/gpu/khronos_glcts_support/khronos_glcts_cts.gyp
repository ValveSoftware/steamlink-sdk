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
      'target_name': 'glcts_common',
      'type': 'static_library',
      'conditions': [
        ['OS=="linux"', {
          'cflags_cc': [
            # WA: Suppress "implicit conversion turns string literal into
            # bool" compile warning from glcShaderIndexingTests.cpp,
            # glcShaderLibraryCase.cpp and glcShaderLoopTests.cpp during
            # Debug build
            # TODO(uartie) fix.
            '-Wno-string-conversion',
          ],
        }],
      ],
      'dependencies': [
        '<(DEPTH)/gpu/khronos_glcts_support/khronos_glcts_framework.gyp:delibs',
        '<(DEPTH)/gpu/khronos_glcts_support/khronos_glcts_framework.gyp:tcutil',
        '<(DEPTH)/gpu/khronos_glcts_support/khronos_glcts_framework.gyp:glutil',
        '<(DEPTH)/gpu/khronos_glcts_support/khronos_glcts_framework.gyp:tcutil_egl',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/khronos_glcts/cts/common',
        ],
      },
      'sources': [
        '<@(glcts_common_sources)',
      ],
    },
    {
      'target_name': 'glcts_gtf_wrapper',
      'type': 'static_library',
      'dependencies': [
        'glcts_common',
        '<(DEPTH)/gpu/khronos_glcts_support/khronos_glcts_framework.gyp:delibs',
        '<(DEPTH)/gpu/khronos_glcts_support/khronos_glcts_framework.gyp:tcutil',
        '<(DEPTH)/gpu/khronos_glcts_support/khronos_glcts_framework.gyp:glutil',
        '<(DEPTH)/gpu/khronos_glcts_support/khronos_glcts_framework.gyp:tcutil_egl',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/khronos_glcts/cts/gtf',
        '<(DEPTH)/third_party/khronos_glcts/GTF_ES/glsl/GTF/Source'
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/khronos_glcts/cts/gtf',
        ],
      },
      'sources': [
        '<@(glcts_gtf_wrapper_srcs)',
      ],
    },
    {
      'target_name': 'glcts_gtf',
      'type': 'static_library',
      'dependencies': [
        'glcts_common', 'glcts_gtf_wrapper',
        '<(DEPTH)/gpu/khronos_glcts_support/khronos_glcts_gtf.gyp:gtf_es',
        '<(DEPTH)/gpu/khronos_glcts_support/khronos_glcts_framework.gyp:delibs',
        '<(DEPTH)/gpu/khronos_glcts_support/khronos_glcts_framework.gyp:tcutil',
        '<(DEPTH)/gpu/khronos_glcts_support/khronos_glcts_framework.gyp:glutil',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/khronos_glcts/cts/gtf',
        ],
      },
      'sources': [
        '<@(glcts_gtf_srcs)',
      ],
    },
    {
      'target_name': 'glcts_es2',
      'type': 'static_library',
      'dependencies': [
        'glcts_common', 'glcts_gtf',
        '<(DEPTH)/gpu/khronos_glcts_support/khronos_glcts_framework.gyp:delibs',
        '<(DEPTH)/gpu/khronos_glcts_support/khronos_glcts_framework.gyp:tcutil',
        '<(DEPTH)/gpu/khronos_glcts_support/khronos_glcts_framework.gyp:glutil',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/khronos_glcts/cts/gles2',
        ],
      },
      'export_dependent_settings': [
        'glcts_common',
      ],
      'sources': [
        '<@(glcts_es2_srcs)',
      ],
    },
  ],
}
