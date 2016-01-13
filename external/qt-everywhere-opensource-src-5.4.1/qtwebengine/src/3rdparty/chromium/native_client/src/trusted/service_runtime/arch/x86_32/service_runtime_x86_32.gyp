# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
        ['OS=="linux"', {
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
