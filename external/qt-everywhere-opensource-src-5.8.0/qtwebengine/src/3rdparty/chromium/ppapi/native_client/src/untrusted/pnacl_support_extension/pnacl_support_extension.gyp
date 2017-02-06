# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  # re: untrusted.gypi -- Though this doesn't really use build_nexe.py or
  # anything, it depends on untrusted nexes from the toolchain and for the shim.
  'includes': [
    '../../../../../build/common_untrusted.gypi',
  ],
  'targets': [
  {
    'target_name': 'pnacl_support_extension',
    'type': 'none',
    'variables': {
      'pnacl_translator_dir%': "<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_translator",
      'pnacl_translator_stamp%': "pnacl_translator.json",
      'pnacl_output_prefix': '<(PRODUCT_DIR)/pnacl/pnacl_public_',
    },
    'conditions': [
      ['disable_nacl==0 and disable_pnacl==0 and disable_nacl_untrusted==0', {
        'dependencies': [
          '../../../../../ppapi/native_client/src/untrusted/pnacl_irt_shim/pnacl_irt_shim.gyp:browser',
        ],
        'sources': [
          'pnacl_component_crx_gen.py',
        ],
        # We could use 'copies', but we want to rename the files
        # in a white-listed way first.  Thus use a script.
        'actions': [
          {
            'action_name': 'generate_pnacl_support_extension',
            'inputs': [
              'pnacl_component_crx_gen.py',
              '<(DEPTH)/native_client/build/package_version/package_version.py',
              # A stamp file representing the contents of pnacl_translator.
              '<(pnacl_translator_dir)/<(pnacl_translator_stamp)',
              '<(DEPTH)/native_client/pnacl/driver/pnacl_info_template.json',
              '<(DEPTH)/native_client/toolchain_revisions/pnacl_translator.json',
            ],
            'outputs': [
              '<(pnacl_output_prefix)pnacl_json',
            ],
            'conditions': [
              # On Windows, for offline testing (i.e., without component updater
              # selecting the platform-specific files with multi-CRXes), we need
              # to stage both x86-32 and x86-64 (because 32-bit chrome on 64-bit
              # windows will need 64-bit nexes).
                ['OS=="win"', {
                    'outputs': [
                      '<(pnacl_output_prefix)x86_32_crtbegin_o',
                      '<(pnacl_output_prefix)x86_32_ld_nexe',
                      '<(pnacl_output_prefix)x86_32_libcrt_platform_a',
                      '<(pnacl_output_prefix)x86_32_libgcc_a',
                      '<(pnacl_output_prefix)x86_32_libpnacl_irt_shim_a',
                      '<(pnacl_output_prefix)x86_32_pnacl_llc_nexe',
                      '<(pnacl_output_prefix)x86_32_pnacl_sz_nexe',
                      '<(pnacl_output_prefix)x86_64_crtbegin_o',
                      '<(pnacl_output_prefix)x86_64_ld_nexe',
                      '<(pnacl_output_prefix)x86_64_libcrt_platform_a',
                      '<(pnacl_output_prefix)x86_64_libgcc_a',
                      '<(pnacl_output_prefix)x86_64_libpnacl_irt_shim_a',
                      '<(pnacl_output_prefix)x86_64_pnacl_llc_nexe',
                      '<(pnacl_output_prefix)x86_64_pnacl_sz_nexe',
                    ],
                    'inputs': [
                      '>(tc_lib_dir_newlib32)/libpnacl_irt_shim_browser.a',
                      '>(tc_lib_dir_newlib64)/libpnacl_irt_shim_browser.a',
                    ],
                    'variables': {
                      'lib_overrides': [
                        # Use the two freshly generated shims.
                        '--lib_override=ia32,>(tc_lib_dir_newlib32)/libpnacl_irt_shim_browser.a,libpnacl_irt_shim.a',
                        '--lib_override=x64,>(tc_lib_dir_newlib64)/libpnacl_irt_shim_browser.a,libpnacl_irt_shim.a',
                      ],
                    },
                }],
                # Non-windows installers only need the matching architecture.
                ['OS!="win"', {
                   'conditions': [
                      ['target_arch=="arm"', {
                        'outputs': [
                          '<(pnacl_output_prefix)arm_crtbegin_o',
                          '<(pnacl_output_prefix)arm_ld_nexe',
                          '<(pnacl_output_prefix)arm_libcrt_platform_a',
                          '<(pnacl_output_prefix)arm_libgcc_a',
                          '<(pnacl_output_prefix)arm_libpnacl_irt_shim_a',
                          '<(pnacl_output_prefix)arm_pnacl_llc_nexe',
                          '<(pnacl_output_prefix)arm_pnacl_sz_nexe',
                        ],
                       'inputs': [
                          '>(tc_lib_dir_newlib_arm)/libpnacl_irt_shim_browser.a',
                        ],
                        'variables': {
                          'lib_overrides': [
                            # Use the freshly generated shim.
                            '--lib_override=arm,>(tc_lib_dir_newlib_arm)/libpnacl_irt_shim_browser.a,libpnacl_irt_shim.a',
                          ],
                        },
                      }],
                      ['target_arch=="mipsel"', {
                        'outputs': [
                          '<(pnacl_output_prefix)mips32_crtbegin_o',
                          '<(pnacl_output_prefix)mips32_ld_nexe',
                          '<(pnacl_output_prefix)mips32_libcrt_platform_a',
                          '<(pnacl_output_prefix)mips32_libgcc_a',
                          '<(pnacl_output_prefix)mips32_libpnacl_irt_shim_a',
                          '<(pnacl_output_prefix)mips32_pnacl_llc_nexe',
                        ],
                        'inputs': [
                          '>(tc_lib_dir_newlib_mips)/libpnacl_irt_shim_browser.a',
                        ],
                        'variables': {
                          'lib_overrides': [
                            # Use the freshly generated shim.
                            '--lib_override=mipsel,>(tc_lib_dir_newlib_mips)/libpnacl_irt_shim_browser.a,libpnacl_irt_shim.a',
                          ],
                        },
                      }],
                      ['target_arch=="ia32"', {
                        'outputs': [
                          '<(pnacl_output_prefix)x86_32_crtbegin_o',
                          '<(pnacl_output_prefix)x86_32_ld_nexe',
                          '<(pnacl_output_prefix)x86_32_libcrt_platform_a',
                          '<(pnacl_output_prefix)x86_32_libgcc_a',
                          '<(pnacl_output_prefix)x86_32_libpnacl_irt_shim_a',
                          '<(pnacl_output_prefix)x86_32_pnacl_llc_nexe',
                          '<(pnacl_output_prefix)x86_32_pnacl_sz_nexe',
                        ],
                        'inputs': [
                          '>(tc_lib_dir_newlib32)/libpnacl_irt_shim_browser.a',
                        ],
                        'variables': {
                          'lib_overrides': [
                            # Use the freshly generated shim.
                            '--lib_override=ia32,>(tc_lib_dir_newlib32)/libpnacl_irt_shim_browser.a,libpnacl_irt_shim.a',
                          ],
                        },
                      }],
                      ['target_arch=="x64"', {
                        'outputs': [
                          '<(pnacl_output_prefix)x86_64_crtbegin_o',
                          '<(pnacl_output_prefix)x86_64_ld_nexe',
                          '<(pnacl_output_prefix)x86_64_libcrt_platform_a',
                          '<(pnacl_output_prefix)x86_64_libgcc_a',
                          '<(pnacl_output_prefix)x86_64_libpnacl_irt_shim_a',
                          '<(pnacl_output_prefix)x86_64_pnacl_llc_nexe',
                          '<(pnacl_output_prefix)x86_64_pnacl_sz_nexe',
                        ],
                        'inputs': [
                          '>(tc_lib_dir_newlib64)/libpnacl_irt_shim_browser.a',
                        ],
                        'variables': {
                          'lib_overrides': [
                            # Use the freshly generated shim.
                            '--lib_override=x64,>(tc_lib_dir_newlib64)/libpnacl_irt_shim_browser.a,libpnacl_irt_shim.a',
                          ],
                        },
                      }],
                  ],
               }],
            ],
            'action': [
              'python', 'pnacl_component_crx_gen.py',
              '--dest=<(PRODUCT_DIR)/pnacl',
              '<@(lib_overrides)',
              '--target_arch=<(target_arch)',
              '--info_template_path=<(DEPTH)/native_client/pnacl/driver/pnacl_info_template.json',
              '--pnacl_translator_path=<(pnacl_translator_dir)',
              '--package_version_path=<(DEPTH)/native_client/build/package_version/package_version.py',
              '--pnacl_package_name=pnacl_translator',
              # ABI Version Number.
              '1',
            ],
          },
        ],
      }],
    ],
  }],
}
