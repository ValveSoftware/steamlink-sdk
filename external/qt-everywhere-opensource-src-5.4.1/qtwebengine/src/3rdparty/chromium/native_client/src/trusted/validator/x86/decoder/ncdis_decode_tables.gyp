# -*- gyp -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="ncdis_decode_tables"', {
        'sources': ['ncdis_decode_tables.c'],
        'cflags!': [
          '-Wextra',
          '-Wswitch-enum',
          '-Wsign-compare'
        ],
        'xcode_settings': {
          'WARNING_CFLAGS!': [
            '-Wextra',
            '-Wswitch-enum',
            '-Wsign-compare'
          ],
        },
      }],
    ],
  },
  'conditions': [
    ['target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'ncdis_decode_tables_x86_32',
          'type': 'static_library',
          'hard_dependency': 1,
          'variables': {
            'target_base': 'ncdis_decode_tables',
          },
        }],
    }],
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'ncdis_decode_tables_x86_64',
          'type': 'static_library',
          'hard_dependency': 1,
          'variables': {
            'target_base': 'ncdis_decode_tables',
            'win_target': 'x64',
          },
        }],
    }],
    ['target_arch=="x64"', {
      'targets': [
        {
          'target_name': 'ncdis_decode_tables_x86_64',
          'type': 'static_library',
          'hard_dependency': 1,
          'variables': {
            'target_base': 'ncdis_decode_tables',
          },
        }],
    }],
  ],
}
