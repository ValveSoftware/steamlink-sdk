# -*- python -*-
#
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'common_sources': [
      'nacl_check.c',
      'nacl_check.h',
      'nacl_find_addrsp.h',
      'nacl_global_secure_random.c',
      'nacl_global_secure_random.h',
      'nacl_host_desc.h',
      'nacl_host_dir.h',
      'nacl_host_desc_common.c',
      'nacl_interruptible_condvar.c',
      'nacl_interruptible_condvar.h',
      'nacl_interruptible_mutex.c',
      'nacl_interruptible_mutex.h',
      'nacl_log.c',
      'nacl_log.h',
      'nacl_secure_random.h',
      'nacl_secure_random_base.h',
      'nacl_secure_random_common.c',
      'nacl_semaphore.h',
      'nacl_sync.h',
      'nacl_sync_checked.c',
      'nacl_sync_checked.h',
      'nacl_threads.h',
      'nacl_time.h',
      'nacl_time_common.c',
      'nacl_timestamp.h',
      'platform_init.c',
      'refcount_base.cc',
    ],
    'conditions': [
      ['OS=="linux" or OS=="android"', {
        'platform_sources': [
          'linux/nacl_clock.c',
          'linux/nacl_host_dir.c',
          'linux/nacl_semaphore.c',
        ],
      }],
      ['OS=="mac"', {
        'platform_sources': [
          'osx/nacl_clock.c',
          'osx/nacl_host_dir.c',
          'osx/nacl_semaphore.c',
        ],
      }],
      ['os_posix==1', {
        'platform_sources': [
          'posix/aligned_malloc.c',
          'posix/condition_variable.c',
          'posix/lock.c',
          'posix/nacl_error.c',
          'posix/nacl_exit.c',
          'posix/nacl_fast_mutex.c',
          'posix/nacl_file_lock.c',
          'posix/nacl_find_addrsp.c',
          'posix/nacl_host_desc.c',
          'posix/nacl_secure_random.c',
          'posix/nacl_thread_id.c',
          'posix/nacl_threads.c',
          'posix/nacl_time.c',
          'posix/nacl_timestamp.c',
        ],
      }],
      ['OS=="win"', {
        'platform_sources': [
          'win/aligned_malloc.c',
          'win/condition_variable.cc',
          'win/lock.cc',
          'win/nacl_clock.c',
          'win/nacl_error.c',
          'win/nacl_exit.c',
          'win/nacl_fast_mutex.c',
          'win/nacl_find_addrsp.c',
          'win/nacl_host_desc.c',
          'win/nacl_host_dir.c',
          'win/lock_impl_win.cc',
          'win/nacl_secure_random.c',
          'win/nacl_semaphore.c',
          'win/nacl_sync_win.cc',
          'win/nacl_threads.c',
          'win/nacl_time.c',
          'win/nacl_timestamp.c',
          'win/port_win.c',
          'win/xlate_system_error.c',
        ],
      }],
    ],
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'target_base': 'none',
    },
    'target_conditions': [
      ['<(os_posix)==1', {
        'cflags': [
          '-Wno-long-long',
        ],
      }],
      ['target_base=="platform_lib"', {
        'sources': [
          '<@(common_sources)',
          '<@(platform_sources)',
        ],
      }],
      ['target_base=="platform_tests"', {
          'sources': [
            'platform_tests.cc',
          ],
          'conditions': [[
            'OS=="win"', {
              'sources': [
                'win/port_win_test.c',
              ],
            }
          ]]
        }
      ]
    ],
  },
  'targets': [
    # ----------------------------------------------------------------------
    {
      'target_name': 'platform',
      'type': 'static_library',
      # tls_edit relies on this which is always built for the host platform.
      'toolsets': ['host', 'target'],
      'variables': {
        'target_base': 'platform_lib',
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio',
      ],
    },
    # ----------------------------------------------------------------------
    {
      'target_name': 'platform_tests',
      'type': 'executable',
      'variables': {
        'target_base': 'platform_tests',
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio',
      ],
    },
    {
      'target_name': 'platform_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libplatform.a',
        'nso_target': 'libplatform.so',
        'build_glibc': 1,
        'build_newlib': 1,
        'build_pnacl_newlib': 1,
        'build_irt': 1,
        'build_nonsfi_helper': 1,
        'sources': [
          'nacl_check.c',
          'nacl_log.c',
          'posix/condition_variable.c',
          'posix/lock.c',
          'posix/nacl_error.c',
          'posix/nacl_exit.c',
          'posix/nacl_thread_id.c',
          'posix/nacl_threads.c',
          'posix/nacl_timestamp.c',
          'nacl_sync_checked.c',
          'refcount_base.cc',
        ]
      },
    },
    # ----------------------------------------------------------------------
  ],
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        # ---------------------------------------------------------------------
        {
          'target_name': 'platform64',
          'type': 'static_library',
          'variables': {
            'target_base': 'platform_lib',
            'win_target': 'x64',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio64',
          ],
        },
        # ---------------------------------------------------------------------
        {
          'target_name': 'platform_tests64',
          'type': 'executable',
          'variables': {
            'target_base': 'platform_tests',
            'win_target': 'x64',
          },
          'sources': [
            'win/test_tls.S',
          ],
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
            '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio64',
          ],
        },
      ],
    }],
  ],
}
