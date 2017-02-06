# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'nacl_sys_private',
      'type': 'none',
      'variables': {
        'nlib_target': 'libnacl_sys_private.a',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_irt': 0,
        'build_pnacl_newlib': 0,
        'build_nonsfi_helper': 1,
      },
      'compile_flags': [
        '-fgnu-inline-asm',
      ],
      'sources': [
        '../../untrusted/irt/irt_query_list.c',
        '../../untrusted/pthread/nc_condvar.c',
        '../../untrusted/pthread/nc_mutex.c',
        '../../untrusted/pthread/nc_thread.c',
        '../../untrusted/pthread/stack_end.c',
        '../../untrusted/valgrind/dynamic_annotations.c',
        '../linux/directory.c',
        '../linux/irt_signal_handling.c',
        '../linux/linux_futex.c',
        '../linux/linux_pthread_private.c',
        '../linux/linux_sys_private.c',
        'irt_icache.c',
        'irt_interfaces.c',
        'irt_random.c',
      ],
    },
  ],
}
