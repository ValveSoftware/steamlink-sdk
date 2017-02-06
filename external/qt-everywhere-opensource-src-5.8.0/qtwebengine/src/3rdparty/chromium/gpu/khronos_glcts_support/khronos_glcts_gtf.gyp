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
      'target_name': 'gtf_es',
      'type': 'static_library',
      'conditions': [
        ['OS=="linux"', {
          'cflags': [
            # WA: Suppress "control reaches end of non-void function" compile
            # warning from GTFTestDriver.c.
            # TODO(uartie) fix.
            '-Wno-return-type',

            # WA: Suppress "incompatible pointer types passing to parameter"
            # compile warning from GTFTestExtension.c.
            # TODO(uartie) fix.
            '-Wno-incompatible-pointer-types',

            # WA: Suppress "passing 'GLint [15]' to parameter of type
            # 'GLuint *' (aka 'unsigned int') converts between pointers to
            # integer types with different sign" compile warning from
            # GTFGL2TestGetAttachedObjects.c
            # TODO(uartie) fix.
            '-Wno-pointer-sign',

            # WA: Suppress "comparison of unsigned expression >= 0 is always
            # true" compile warning from GTFgl.c
            # TODO(uartie) fix.
            '-Wno-tautological-compare',

            # WA: Suppress "equality comparison with extraneous parentheses"
            # compile warning from GTFgl.c
            # TODO(uartie) fix.
            '-Wno-parentheses-equality',
          ],
        }],
      ],
      'dependencies': [
        '<(DEPTH)/gpu/khronos_glcts_support/khronos_glcts_framework.gyp:debase',
        '<(DEPTH)/gpu/khronos_glcts_support/khronos_glcts_cts.gyp:glcts_gtf_wrapper',
        '<(DEPTH)/third_party/expat/expat.gyp:expat',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/khronos_glcts/GTF_ES/glsl/GTF/Source',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/khronos_glcts/GTF_ES/glsl/GTF/Source',
        ],
      },
      'sources': [
        '<@(gtf_core_srcs)',
        '<@(gtf_gl_core_srcs)',
        '<@(gtf_gles2_srcs)',
        '<@(gtf_gles2_es_only_srcs)',
      ],
    },
  ],
}
