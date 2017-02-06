# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'elf_loader',
      'type': 'none',
      'variables': {
        'nlib_target': 'libelf_loader.a',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_irt': 0,
        'build_pnacl_newlib': 0,
        'build_nonsfi_helper': 1,
      },
      'sources': [
        'elf_loader.c',
      ],
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
      ],
    },
    {
      'target_name': 'nonsfi_loader',
      'type': 'none',
      'variables': {
        'nexe_target': 'nonsfi_loader',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_irt': 0,
        'build_pnacl_newlib': 0,
        'build_nonsfi_helper': 1,
        'conditions': [
          ['target_arch=="ia32" or target_arch=="x64"', {
            'extra_deps_newlib32_nonsfi': [
              '>(tc_lib_dir_nonsfi_helper32)/libgio.a',
              '>(tc_lib_dir_nonsfi_helper32)/libplatform.a',
            ],
          }],
          ['target_arch=="arm"', {
            'extra_deps_newlib_arm_nonsfi': [
              '>(tc_lib_dir_nonsfi_helper_arm)/libgio.a',
              '>(tc_lib_dir_nonsfi_helper_arm)/libplatform.a',
            ],
          }],
        ],
      },
      'sources': [
        'elf_loader_main.c',
      ],
      'link_flags': [
        '-lelf_loader',
        '-lplatform',
        '-lgio',
      ],
      'dependencies': [
        'elf_loader',
        '<(DEPTH)/native_client/src/nonsfi/irt/irt.gyp:nacl_sys_private',
        '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:nacl_lib_newlib',
      ],
    },
  ],
}
