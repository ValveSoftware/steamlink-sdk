# -*- python -*-
# Copyright 2008 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables':{
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="nrd_xfer"', {
        'sources': [
          'nacl_desc_base.c',
          'nacl_desc_base.h',
          'nacl_desc_cond.c',
          'nacl_desc_cond.h',
          'nacl_desc_custom.c',
          'nacl_desc_dir.c',
          'nacl_desc_dir.h',
          'nacl_desc_effector.h',
          'nacl_desc_effector_trusted_mem.c',
          'nacl_desc_effector_trusted_mem.h',
          'nacl_desc_imc.c',
          'nacl_desc_imc.h',
          'nacl_desc_imc_shm.c',
          'nacl_desc_imc_shm.h',
          'nacl_desc_invalid.c',
          'nacl_desc_invalid.h',
          'nacl_desc_io.c',
          'nacl_desc_io.h',
          'nacl_desc_mutex.c',
          'nacl_desc_mutex.h',
          'nacl_desc_null.c',
          'nacl_desc_null.h',
          'nacl_desc_quota.c',
          'nacl_desc_quota.h',
          'nacl_desc_quota_interface.c',
          'nacl_desc_quota_interface.h',
          'nacl_desc_semaphore.c',
          'nacl_desc_semaphore.h',
          'nacl_desc_sync_socket.c',
          'nacl_desc_sync_socket.h',
          'nrd_all_modules.c',
          'nrd_all_modules.h',
          # nrd_xfer_obj = env_no_strict_aliasing.ComponentObject('nrd_xfer.c')
          'nrd_xfer.c',
          'nrd_xfer.h',
          'nrd_xfer_intern.h',
        ],
        # TODO(bsy,bradnelson): when gyp can do per-file flags, make
        # -fno-strict-aliasing and -Wno-missing-field-initializers
        # apply only to nrd_xfer.c
        'cflags': [
          '-fno-strict-aliasing',
          '-Wno-missing-field-initializers'
        ],
        'xcode_settings': {
          'WARNING_CFLAGS': [
            '-fno-strict-aliasing',
            '-Wno-missing-field-initializers'
          ]
        },
        'conditions': [
          ['os_posix==1', {
            'sources': [
              'posix/nacl_desc.c',
            ],
          }],
          ['OS=="mac"', {
            'sources': [
              'osx/nacl_desc_imc_shm_mach.c',
              'osx/nacl_desc_imc_shm_mach.h',
            ],
          }],
          ['OS=="win"', { 'sources': [
              'win/nacl_desc.c',
          ]}],
          ['OS=="win"',
            # String-based bound socket implementation.
            {'sources': [
              'nacl_desc_conn_cap.c',
              'nacl_desc_imc_bound_desc.c',
              'nacl_makeboundsock.c',
             ],
            },
            # FD-based bound socket implementation.
            {
              'sources': [
                'posix/nacl_desc_conn_cap.c',
                'posix/nacl_desc_imc_bound_desc.c',
                'posix/nacl_makeboundsock.c',
              ],
            }
          ],
        ],
      }],
    ],
  },
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'nrd_xfer64',
          'type': 'static_library',
          'variables': {
            'target_base': 'nrd_xfer',
            'win_target': 'x64',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/imc/imc.gyp:imc64',
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
            '<(DEPTH)/native_client/src/trusted/nacl_base/nacl_base.gyp:nacl_base64',
          ],
        },
      ],
    }],
  ],
  'targets': [
    {
      'target_name': 'nrd_xfer',
      'type': 'static_library',
      'variables': {
        'target_base': 'nrd_xfer',
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/imc/imc.gyp:imc',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/trusted/nacl_base/nacl_base.gyp:nacl_base',
      ],
    },
  ],
}
