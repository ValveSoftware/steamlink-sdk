# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/common_untrusted.gypi',
    'mojo_public.gypi',
  ],
  'target_defaults' : {
   'include_dirs': [
     '..',
   ],
  },
  'targets': [
    {
      # GN version: //mojo/public/c/system
      'target_name': 'mojo_public_system_nacl',
      'type': 'none',
      'variables': {
        'nacl_untrusted_build': 1,
        'nlib_target': 'libmojo_public_system_nacl.a',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_irt': 1,
        'build_pnacl_newlib': 0,
        'build_nonsfi_helper': 1,
      },
      'sources': [
        '<@(mojo_public_system_sources)',
      ],
      'defines': [
        'MOJO_SYSTEM_IMPLEMENTATION',
      ],
    },
    {
      # GN version: //mojo/public/cpp/bindings
      'target_name': 'mojo_cpp_bindings_nacl',
      'type': 'none',
      'variables': {
        'nacl_untrusted_build': 1,
        'nlib_target': 'libmojo_cpp_bindings_nacl.a',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_irt': 1,
        'build_pnacl_newlib': 0,
        'build_nonsfi_helper': 1,
      },
      'dependencies': [
        '../base/base_nacl.gyp:base_nacl',
        '../base/base_nacl.gyp:base_nacl_nonsfi',
        '../mojo/mojo_public.gyp:mojo_interface_bindings_cpp_sources',
        'mojo_cpp_system_nacl',
      ],
      'sources': [
        '<@(mojo_cpp_bindings_sources)',

        # This comes from the mojo_interface_bindings_cpp_sources dependency.
        '>@(mojom_generated_sources)',
      ],
    },
    {
      # GN version: //mojo/public/cpp/system
      'target_name': 'mojo_cpp_system_nacl',
      'type': 'none',
      'variables': {
        'nacl_untrusted_build': 1,
        'nlib_target': 'libmojo_cpp_system_nacl.a',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_irt': 1,
        'build_pnacl_newlib': 0,
        'build_nonsfi_helper': 1,
      },
      'sources': [
        '<@(mojo_cpp_system_sources)',
      ],
      'dependencies': [
        '../base/base_nacl.gyp:base_nacl',
        'mojo_public_system_nacl',
      ],
    },
  ],
}
