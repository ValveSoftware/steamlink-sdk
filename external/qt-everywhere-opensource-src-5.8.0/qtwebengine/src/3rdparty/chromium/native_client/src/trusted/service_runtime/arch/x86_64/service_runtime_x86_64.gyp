# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'service_runtime_x86_64',
      'type': 'static_library',
      'variables': {
        'win_target': 'x64',
      },
      'sources': [
        'nacl_app_64.c',
        'nacl_switch_64.S',
        'nacl_switch_to_app_64.c',
        'nacl_syscall_64.S',
        'nacl_tls_64.c',
        'sel_ldr_x86_64.c',
        'sel_rt_64.c',
        'tramp_64.S',
      ],
      'defines': [
        # For now, we are only supporting the the zero-based sandbox
        # build in scons.
        'NACL_X86_64_ZERO_BASED_SANDBOX=0',
      ],
      'conditions': [
        ['OS=="mac"', {
          'sources' : [
            '../../osx/nacl_signal_64.c',
            'sel_addrspace_posix_x86_64.c',
          ] },
        ],
        ['OS=="linux"', {
          'sources' : [
            '../../linux/nacl_signal_64.c',
            'sel_addrspace_posix_x86_64.c',
          ] },
        ],
        ['OS=="win"', {
          'sources' : [
            '../../win/exception_patch/exit_fast.S',
            '../../win/exception_patch/intercept.S',
            '../../win/exception_patch/ntdll_patch.c',
            '../../win/nacl_signal_64.c',
            'nacl_switch_unwind_win.asm',
            'sel_addrspace_win_x86_64.c',
            'fnstcw.S',
            'fxsaverstor.S',
          ] },
        ],
      ],
      # We assemble the .asm assembly file with the Microsoft
      # assembler because we need to generate x86-64 Windows unwind
      # info, which the GNU assembler we use elsewhere does not
      # support.
      'rules': [
        {
          'rule_name': 'Assemble',
          'msvs_cygwin_shell': 0,
          'msvs_quote_cmd': 0,
          'extension': 'asm',
          'inputs': [],
          'outputs': [
            '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).obj',
          ],
          'action': [
            'ml64',
            '/nologo',
            '/Fo', '<(INTERMEDIATE_DIR)\<(RULE_INPUT_ROOT).obj',
            '/c', '<(RULE_INPUT_PATH)',
          ],
          'process_outputs_as_sources': 1,
          'message':
              'Assembling <(RULE_INPUT_PATH) to ' \
              '<(INTERMEDIATE_DIR)\<(RULE_INPUT_ROOT).obj.',
        },
      ],
    },
  ],
}
