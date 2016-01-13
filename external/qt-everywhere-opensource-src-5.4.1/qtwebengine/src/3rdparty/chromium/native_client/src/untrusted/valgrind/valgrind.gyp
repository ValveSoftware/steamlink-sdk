# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'conditions': [
    ['target_arch!="arm"', {
      'targets' : [
        {
          'target_name': 'valgrind_lib',
          'type': 'none',
          'dependencies': [
            '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
          ],
          'variables': {
            'nlib_target': 'libvalgrind.a',
            'nso_target': 'libvalgrind.so',
            'build_glibc': 1,
            'build_newlib': 1,
          },
          'sources': [
            'valgrind_interceptors.c',
          ],
        },
        {
          'target_name': 'dynamic_annotations_lib',
          'type': 'none',
          'dependencies': [
            '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
          ],
          'variables': {
            'nlib_target': 'libdynamic_annotations.a',
            'nso_target': 'libdynamic_annotations.so',
            'build_glibc': 1,
            'build_newlib': 0,
          },
          'sources': [
            'dynamic_annotations.c',
          ],
        }
      ],
    }],
  ],
}
