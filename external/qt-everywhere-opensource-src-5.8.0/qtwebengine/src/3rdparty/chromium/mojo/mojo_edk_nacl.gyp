# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/common_untrusted.gypi',
    'mojo_edk.gypi',
  ],
  'target_defaults' : {
   'include_dirs': [
     '..',
   ],
  },
  'targets': [
    {
      # GN version: //mojo/edk/system
      'target_name': 'mojo_system_impl_nacl',
      'type': 'none',
      'variables': {
        'nacl_untrusted_build': 1,
        'nlib_target': 'libmojo_system_impl_nacl.a',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_irt': 1,
        'build_pnacl_newlib': 0,
        'build_nonsfi_helper': 0,
      },
      'dependencies': [
        '../base/base_nacl.gyp:base_nacl',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        'mojo_public.gyp:mojo_public_system',
      ],
      'defines': [
        'MOJO_SYSTEM_IMPL_IMPLEMENTATION',
      ],
      'sources': [
        '<@(mojo_edk_ports_sources)',
        '<@(mojo_edk_system_impl_sources)',
      ],
      'conditions': [
        ['OS=="android" or chromeos==1', {
          'defines': [
            'MOJO_EDK_LEGACY_PROTOCOL',
          ],
        }],
      ],
    },
    {
      'target_name': 'mojo_system_impl_nacl_nonsfi',
      'type': 'none',
      'variables': {
        'nacl_untrusted_build': 1,
        'nlib_target': 'libmojo_system_impl_nacl_nonsfi.a',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_irt': 0,
        'build_pnacl_newlib': 0,
        'build_nonsfi_helper': 1,
      },
      'dependencies': [
        '../base/base_nacl.gyp:base_nacl_nonsfi',
        'mojo_public.gyp:mojo_public_system',
      ],
      'defines': [
        'MOJO_SYSTEM_IMPL_IMPLEMENTATION',
      ],
      'sources': [
        '<@(mojo_edk_ports_sources)',
        '<@(mojo_edk_system_impl_sources)',
        '<@(mojo_edk_system_impl_nacl_nonsfi_sources)',
      ],
      'conditions': [
        ['OS=="android" or chromeos==1', {
          'defines': [
            'MOJO_EDK_LEGACY_PROTOCOL',
          ],
        }],
      ],
    },
  ],
}
