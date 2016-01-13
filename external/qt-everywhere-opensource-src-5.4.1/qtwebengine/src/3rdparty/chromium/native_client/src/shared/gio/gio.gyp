# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'variables': {
    'common_sources': [
      'gio.c',
      'gio_mem.c',
      'gprintf.c',
      'gio_mem_snapshot.c',
    ],
    'trusted_sources': [
      '<@(common_sources)',
      'gio_pio.c',
    ],
  },
  'targets': [
    {
      'target_name': 'gio',
      'type': 'static_library',
      # tls_edit relies on gio which is always built for the host platform.
      'toolsets': ['host', 'target'],
      'sources': [
        '<@(trusted_sources)',
      ],
    },
    {
      'target_name': 'gio_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libgio.a',
        'nso_target': 'libgio.so',
        'build_glibc': 1,
        'build_newlib': 1,
        'build_pnacl_newlib': 1,
        'build_irt': 1,
        'sources': ['<@(common_sources)']
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
  ],
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'gio64',
          'type': 'static_library',
            'sources': [
              '<@(trusted_sources)',
            ],
          'variables': {
            'win_target': 'x64',
          },
        }
      ],
    }],
  ],
}
