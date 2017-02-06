# Copyright 2009 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'service_runtime_x86_32',
      'type': 'static_library',
      'sources': [
        'nacl_app_32.c',
        'nacl_switch_32.S',
        'nacl_switch_all_regs_32.c',
        'nacl_switch_all_regs_asm_32.S',
        'nacl_switch_to_app_32.c',
        'nacl_syscall_32.S',
        'nacl_tls_32.c',
        'sel_addrspace_x86_32.c',
        'sel_ldr_x86_32.c',
        'sel_rt_32.c',
        'springboard.S',
        'tramp_32.S',
      ],
      # VS2010 does not correctly incrementally link obj files generated from
      # asm files. This flag disables UseLibraryDependencyInputs to avoid
      # this problem.
      'msvs_2010_disable_uldi_when_referenced': 1,
      'conditions': [
        ['OS=="mac"', {
          'sources' : [
            '../../osx/nacl_signal_32.c',
          ] },
        ],
        ['OS=="linux" or OS=="android"', {
          'sources' : [
            '../../linux/nacl_signal_32.c',
          ] },
        ],
        ['OS=="win"', {
          'sources' : [
            '../../win/nacl_signal_32.c',
          ] },
        ],
      ],
    },
  ],
}
