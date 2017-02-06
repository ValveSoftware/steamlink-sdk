# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../../native_client/build/untrusted.gypi',
  ],
  'targets': [
    {
      # The full library, which PNaCl uses for offline .pexe -> .nexe.
      # We keep the target names in this file short to avoid having really long
      # path names on Windows.
      # https://code.google.com/p/nativeclient/issues/detail?id=3846
      'target_name': 'aot',
      'type': 'none',
      'variables': {
        'nlib_target': 'libpnacl_irt_shim.a',
        'out_pnacl_newlib_arm': '>(tc_lib_dir_pnacl_translate)/lib-arm/>(nlib_target)',
        'out_pnacl_newlib_x86_32': '>(tc_lib_dir_pnacl_translate)/lib-x86-32/>(nlib_target)',
        'out_pnacl_newlib_x86_64': '>(tc_lib_dir_pnacl_translate)/lib-x86-64/>(nlib_target)',
        'out_pnacl_newlib_mips': '>(tc_lib_dir_pnacl_translate)/lib-mips32/>(nlib_target)',
        # TODO(hidehiko): When crrev.com/524573002 is submitted and NaCl
        # repository is rolled out with the CL, then we can simply remove this
        # variable, as it is no longer used.
        'out_pnacl_newlib_x86_32_nonsfi': '>(tc_lib_dir_pnacl_translate)/lib-x86-32-nonsfi/>(nlib_target)',
        'out_newlib32_nonsfi': '>(tc_lib_dir_pnacl_translate)/lib-x86-32-nonsfi/>(nlib_target)',
        'out_pnacl_newlib_arm_nonsfi': '>(tc_lib_dir_pnacl_translate)/lib-arm-nonsfi/>(nlib_target)',
        'out_newlib_arm_nonsfi': '>(tc_lib_dir_pnacl_translate)/lib-arm-nonsfi/>(nlib_target)',
        'build_pnacl_newlib': 1,
        'pnacl_native_biased': 1,
        'enable_x86_32': 1,
        'enable_x86_64': 1,
        'enable_arm': 1,
        'enable_mips': 1,
        'enable_x86_32_nonsfi': 1,
        'enable_arm_nonsfi': 1,
        'sources': [
          'irt_shim_ppapi.c',
          'pnacl_shim.c',
          'shim_entry.c',
          'shim_ppapi.c',
        ],
        'extra_args': [
          '--strip-debug',
        ],
        # Indicate that this variant of the shim library should not depend on
        # the unstable/private IRT hook interface.
        'compile_flags': [
          '-DPNACL_SHIM_AOT',
        ],
      },
    },
    # Smaller shim library for PNaCl in-browser translation.
    # Uses an unstable IRT hook interface to get the shim from the IRT itself.
    # If we ever change that hook interface or change the in-IRT shim's ABI,
    # we would need to clear the translation cache to match the new IRT.
    {
      'target_name': 'browser',
      'type': 'none',
      'variables': {
        # Use a different name from the AOT variant. Have the
        # pnacl_support_extension rename this to the canonical name when
        # copying to the final target directory.
        'nlib_target': 'libpnacl_irt_shim_browser.a',
        'build_newlib': 1,
        'sources': [
          'shim_entry.c',
          'shim_ppapi.c',
        ],
        'extra_args': [
          '--strip-debug',
        ],
      },
    },
    {
      # Second half of shim library for PNaCl in-browser translation.
      # This half goes into the IRT and is returned by the unstable
      # IRT hook interface.
      'target_name': 'irt',
      'type': 'none',
      'variables': {
        'nlib_target': 'libpnacl_irt_shim_for_irt.a',
        'build_glibc': 0,
        'build_newlib': 0,
        # Unlike the above, build this the way the IRT is built so that the
        # output library directories match the IRT linking search paths.
        'build_irt': 1,
        'sources': [
          'irt_shim_ppapi.c',
          'pnacl_shim.c',
        ],
      },
    },
  ],
}
