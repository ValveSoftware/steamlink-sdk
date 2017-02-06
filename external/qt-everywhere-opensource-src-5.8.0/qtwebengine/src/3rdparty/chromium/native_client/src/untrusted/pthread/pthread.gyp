# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'variables': {
    'common_sources': [
      'nc_thread.c',
      'nc_mutex.c',
      'nc_condvar.c',
      'nc_rwlock.c',
      'nc_semaphore.c',
      'nc_init_irt.c',
      'stack_end.c',
      '../valgrind/dynamic_annotations.c',
    ],
  },
  'targets' : [
    {
      'target_name': 'pthread_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libpthread.a',
        'build_glibc': 0,
        'build_newlib': 1,
        'build_pnacl_newlib': 1,
      },
      'sources': ['<@(common_sources)'],
    },
  ],
}
