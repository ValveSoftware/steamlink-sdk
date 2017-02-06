# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/common_untrusted.gypi',
  ],
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'content_common_nacl_nonsfi',
          'type': 'none',
          'variables': {
            'nacl_untrusted_build': 1,
            'nlib_target': 'libcontent_common_nacl_nonsfi.a',
            'build_glibc': 0,
            'build_newlib': 0,
            'build_irt': 0,
            'build_pnacl_newlib': 0,
            'build_nonsfi_helper': 1,

            'sources': [
              'common/sandbox_linux/sandbox_init_linux.cc',
              'common/sandbox_linux/sandbox_seccomp_bpf_linux.cc',
              'common/send_zygote_child_ping_linux.cc',
              'public/common/content_switches.cc',
            ],
          },
          'defines': [
            'USE_SECCOMP_BPF=1',
          ],
          'dependencies': [
            '../base/base_nacl.gyp:base_nacl_nonsfi',
          ],
        },
      ],
    }],
  ],
}
