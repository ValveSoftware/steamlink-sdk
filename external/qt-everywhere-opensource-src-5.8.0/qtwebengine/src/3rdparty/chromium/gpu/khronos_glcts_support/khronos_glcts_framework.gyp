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
      'target_name': 'khronos_glcts_framework',
      'type': 'static_library',
      'dependencies': [
        'delibs', 'qphelper', 'tcutil', 'glutil', 'tcutil_egl',
      ],
      'export_dependent_settings': [
        'delibs', 'qphelper', 'tcutil', 'glutil', 'tcutil_egl',
      ],
    },
    {
      'target_name': 'delibs',
      'type': 'static_library',
      'dependencies': [
        'debase', 'depool', 'dethread', 'deutil', 'decpp',
      ],
      'export_dependent_settings': [
        'debase', 'depool', 'dethread', 'deutil', 'decpp',
      ],
    },
    {
      'target_name': 'debase',
      'type': 'static_library',
      'conditions': [
        ['OS=="linux"', {
          'cflags': [
            # WA: Suppress "implicit declaration of function '__assert_fail'
            # is invalid in C99" warning from deDefs.c for Release compiles.
            # TODO(uartie) fix.
            '-Wno-implicit-function-declaration',
          ],
        }],
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/khronos_glcts/framework/delibs/debase',
        ],
      },
      'sources': [
        '<@(debase_srcs)',
      ],
    },
    {
      'target_name': 'depool',
      'type': 'static_library',
      'dependencies': [
        'debase',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/khronos_glcts/framework/delibs/depool',
        ],
      },
      'sources': [
        '<@(depool_srcs)',
      ],
    },
    {
      'target_name': 'dethread',
      'type': 'static_library',
      'dependencies': [
        'debase',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/khronos_glcts/framework/delibs/dethread',
        ],
      },
      'sources': [
        '<@(dethread_srcs)',
      ],
      'conditions': [
        ['OS=="linux"', {
          'sources': [
            '<@(dethread_unix_srcs)',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/khronos_glcts/framework/delibs/dethread',
          ],
        }],
      ]
    },
    {
      'target_name': 'deutil',
      'type': 'static_library',
      'dependencies': [
        'debase', 'depool', 'dethread',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/khronos_glcts/framework/delibs/deutil',
        ],
      },
      'sources': [
        '<@(deutil_srcs)',
      ],
    },
    {
      'target_name': 'decpp',
      'type': 'static_library',
      'dependencies': [
        'debase', 'depool', 'dethread', 'deutil',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/khronos_glcts/framework/delibs/decpp',
        ],
      },
      'sources': [
        '<@(decpp_srcs)',
      ],
    },
    {
      'target_name': 'qphelper',
      'type': 'static_library',
      'defines': [
        'QP_SUPPORT_PNG',
      ],
      'dependencies': [
        'debase', 'depool', 'dethread', 'deutil',
        '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/khronos_glcts/framework/qphelper',
        ],
      },
      'sources': [
        '<@(qphelper_srcs)',
      ],
    },
    {
      'target_name': 'tcutil',
      'type': 'static_library',
      'conditions': [
        ['OS=="linux"', {
          'cflags_cc': [
            # WA: Suppress "cast to 'const unsigned char *' from smaller
            # integer type 'deUint32' (aka 'unsigned int')" compile warning
            # from tcuRandomValueIterator.hpp.
            # TODO(uartie) fix.
            '-Wno-int-to-pointer-cast',
          ],
        }],
      ],
      'dependencies': [
        'delibs', 'qphelper',
        '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/khronos_glcts/framework/delibs/libpng', #png.hpp
      ],
      'export_dependent_settings': [
        'qphelper',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/khronos_glcts/framework/common',
        ],
      },
      'sources': [
        '<@(tcutil_srcs)',
      ],
    },
    {
      'target_name': 'glwrapper',
      'type': 'static_library',
      'dependencies': [
        'delibs',
        '<(DEPTH)/gpu/gpu.gyp:gles2_c_lib_nocheck',
        '<(DEPTH)/gpu/gpu.gyp:command_buffer_service',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/khronos_glcts/framework/opengl/wrapper',
        ],
      },
      'sources': [
        '<@(glwrapper_srcs)',
      ],
    },
    {
      'target_name': 'glutil',
      'type': 'static_library',
      'dependencies': [
        'glwrapper', 'delibs', 'tcutil',
      ],
      'export_dependent_settings': [
        'glwrapper',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/khronos_glcts/framework/opengl',
        ],
      },
      'sources': [
        '<@(glutil_srcs)',
      ],
    },
    {
      'target_name': 'tcutil_egl',
      'type': 'static_library',
      'conditions': [
        ['OS=="linux"', {
          'cflags_cc': [
            # WA: Suppress "cast to 'void *' from smaller
            # integer type 'glw::GLuint' (aka 'unsigned int')" compile
            # warning from tcuEglPlatform.cpp.
            # TODO(uartie) fix.
            '-Wno-int-to-void-pointer-cast',
          ],
        }],
      ],
      'dependencies': [
        'delibs', 'tcutil', 'glwrapper',
        # TODO: We may want to phase out the old gles2_conform support in preference
        # of this implementation.  So eventually we'll need to move the egl_native
        # stuff here or to a shareable location/path.
        '<(DEPTH)/gpu/gles2_conform_support/gles2_conform_support.gyp:egl_native',
        '<(DEPTH)/third_party/khronos/khronos.gyp:khronos_headers',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/khronos_glcts/framework/opengl',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/khronos_glcts/framework/egl',
        ],
      },
      'sources': [
        '<@(tcutil_egl_srcs)',
      ],
    },
  ],
}
