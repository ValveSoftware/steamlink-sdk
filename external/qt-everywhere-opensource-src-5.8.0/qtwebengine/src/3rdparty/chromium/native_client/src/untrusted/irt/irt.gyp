# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'variables': {
    'irt_sources': [
# irt_support_sources
      'irt_entry.c',
      'irt_interfaces.c',
      'irt_malloc.c',
      'irt_private_pthread.c',
      'irt_private_tls.c',
      'irt_query_list.c',
# irt_common_interfaces
      'irt_basic.c',
      'irt_code_data_alloc.c',
      'irt_fdio.c',
      'irt_filename.c',
      'irt_memory.c',
      'irt_dyncode.c',
      'irt_thread.c',
      'irt_futex.c',
      'irt_mutex.c',
      'irt_cond.c',
      'irt_sem.c',
      'irt_tls.c',
      'irt_blockhook.c',
      'irt_clock.c',
      'irt_dev_getpid.c',
      'irt_exception_handling.c',
      'irt_dev_list_mappings.c',
      'irt_random.c',
# support_srcs
      # We also get nc_init_private.c, nc_thread.c and nc_tsd.c via
      # #includes of .c files.
      '../pthread/nc_mutex.c',
      '../pthread/nc_condvar.c',
      '../nacl/sys_private.c',
      '../valgrind/dynamic_annotations.c',
    ],
# On x86-64 we build the IRT with sandbox base-address hiding. This means that
# all of its components must be built with LLVM's assembler. Currently the
# unwinder library is built with nacl-gcc and so does not use base address
# hiding. The IRT does not use exceptions, so we do not actually need the
# unwinder at all. To prevent it from being linked into the IRT, we provide
# stub implementations of its functions that are referenced from libc++abi
# (which is build with exception handling enabled) and from crtbegin.c.
    'stub_sources': [
      '../../../pnacl/support/bitcode/unwind_stubs.c',
      'frame_info_stubs.c',
    ],
    'irt_nonbrowser': [
      'irt_core_resource.c',
      'irt_entry_core.c',
      'irt_pnacl_translator_common.c',
      'irt_pnacl_translator_compile.c',
      'irt_pnacl_translator_link.c',
    ],
  },
  'targets': [
    {
      'target_name': 'irt_core_nexe',
      'type': 'none',
      'variables': {
        'nexe_target': 'irt_core',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_irt': 1,
      },
      'sources': ['<@(irt_nonbrowser)'],
      'link_flags': [
        '-Wl,--start-group',
        '-lirt_browser',
        '-limc_syscalls',
        '-lplatform',
        '-lgio',
        '-Wl,--end-group',
        '-lm',
      ],
      'dependencies': [
        'irt_browser_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/tools/tls_edit/tls_edit.gyp:tls_edit#host',
        '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:imc_syscalls_lib',
        '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:nacl_lib_newlib',
      ],
    },
    {
      'target_name': 'irt_browser_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libirt_browser.a',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_irt': 1,
      },
      'sources': ['<@(irt_sources)','<@(stub_sources)'],
      'dependencies': [
        '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:nacl_lib_newlib',
      ],
    },
  ],
}
