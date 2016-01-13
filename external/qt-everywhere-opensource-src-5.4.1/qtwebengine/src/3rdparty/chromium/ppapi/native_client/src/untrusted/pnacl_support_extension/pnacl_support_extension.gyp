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
    'target_name': 'untar_pnacl_translator',
    'type': 'none',
    'actions': [{
      'action_name': 'Untar pnacl_translator',
      'description': 'Untar pnacl_translator',
      'inputs': [
        '<(DEPTH)/native_client/build/package_version/package_version.py',
        '<(DEPTH)/native_client/toolchain/.tars/<(TOOLCHAIN_OS)_x86/pnacl_translator.json',
      ],
      'outputs': ['<(SHARED_INTERMEDIATE_DIR)/<(TOOLCHAIN_OS)_x86/pnacl_translator/pnacl_translator.json'],
      'action': [
        'python',
        '<(DEPTH)/native_client/build/package_version/package_version.py',
        '--quiet',
        '--packages', 'pnacl_translator',
        '--tar-dir', '<(DEPTH)/native_client/toolchain/.tars',
        '--dest-dir', '<(SHARED_INTERMEDIATE_DIR)',
        'extract',
      ],
    }],
  },
  {
    'target_name': 'pnacl_support_extension',
    'type': 'none',
    'conditions': [
      ['disable_nacl==0 and disable_pnacl==0 and disable_nacl_untrusted==0', {
        'dependencies': [
          '../../../../../ppapi/native_client/src/untrusted/pnacl_irt_shim/pnacl_irt_shim.gyp:shim_browser',
          '../../../../../native_client/tools.gyp:prep_toolchain',
          'untar_pnacl_translator',
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
              # A stamp file representing the contents of pnacl_translator.
              '<(SHARED_INTERMEDIATE_DIR)/<(TOOLCHAIN_OS)_x86/pnacl_translator/pnacl_translator.json',
              '<(DEPTH)/native_client/pnacl/driver/pnacl_info_template.json',
              '<(DEPTH)/native_client/toolchain_revisions/pnacl_newlib.json',
            ],
            'conditions': [
                # On windows we need both ia32 and x64.
                ['OS=="win"', {
                    'outputs': [
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_pnacl_json',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_crtbegin_o',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_ld_nexe',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_libcrt_platform_a',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_libgcc_a',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_libgcc_eh_a',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_libpnacl_irt_shim_a',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_pnacl_llc_nexe',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_crtbegin_o',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_ld_nexe',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_libcrt_platform_a',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_libgcc_a',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_libgcc_eh_a',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_libpnacl_irt_shim_a',
                      '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_pnacl_llc_nexe',
                    ],
                    'inputs': [
                      '>(tc_lib_dir_pnacl_translate)/lib-x86-32/for_browser/libpnacl_irt_shim.a',
                      '>(tc_lib_dir_pnacl_translate)/lib-x86-64/for_browser/libpnacl_irt_shim.a',
                    ],
                    'variables': {
                      'lib_overrides': [
                        # Use the two freshly generated shims.
                        '--lib_override=ia32,>(tc_lib_dir_pnacl_translate)/lib-x86-32/for_browser/libpnacl_irt_shim.a',
                        '--lib_override=x64,>(tc_lib_dir_pnacl_translate)/lib-x86-64/for_browser/libpnacl_irt_shim.a',
                      ],
                    },
                }],
                # Non-windows installers only need the matching architecture.
                ['OS!="win"', {
                   'conditions': [
                      ['target_arch=="arm"', {
                        'outputs': [
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_pnacl_json',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_arm_crtbegin_o',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_arm_ld_nexe',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_arm_libcrt_platform_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_arm_libgcc_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_arm_libgcc_eh_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_arm_libpnacl_irt_shim_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_arm_pnacl_llc_nexe',
                        ],
                        'inputs': [
                          '>(tc_lib_dir_pnacl_translate)/lib-arm/for_browser/libpnacl_irt_shim.a',
                        ],
                        'variables': {
                          'lib_overrides': [
                            # Use the freshly generated shim.
                            '--lib_override=arm,>(tc_lib_dir_pnacl_translate)/lib-arm/for_browser/libpnacl_irt_shim.a',
                          ],
                        },
                      }],
                      ['target_arch=="mipsel"', {
                        'outputs': [
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_pnacl_json',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_mips32_crtbegin_o',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_mips32_ld_nexe',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_mips32_libcrt_platform_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_mips32_libgcc_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_mips32_libgcc_eh_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_mips32_libpnacl_irt_shim_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_mips32_llc_nexe',
                        ],
                        'inputs': [
                          '>(tc_lib_dir_pnacl_translate)/lib-mips32/for_browser/libpnacl_irt_shim.a',
                        ],
                        'variables': {
                          'lib_overrides': [
                            # Use the freshly generated shim.
                            '--lib_override=mipsel,>(tc_lib_dir_pnacl_translate)/lib-mips32/for_browser/libpnacl_irt_shim.a',
                          ],
                        },
                      }],
                      ['target_arch=="ia32"', {
                        'outputs': [
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_pnacl_json',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_crtbegin_o',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_ld_nexe',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_libcrt_platform_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_libgcc_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_libgcc_eh_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_libpnacl_irt_shim_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_32_pnacl_llc_nexe',
                        ],
                        'inputs': [
                          '>(tc_lib_dir_pnacl_translate)/lib-x86-32/for_browser/libpnacl_irt_shim.a',
                        ],
                        'variables': {
                          'lib_overrides': [
                            # Use the freshly generated shim.
                            '--lib_override=ia32,>(tc_lib_dir_pnacl_translate)/lib-x86-32/for_browser/libpnacl_irt_shim.a',
                          ],
                        },
                      }],
                      ['target_arch=="x64"', {
                        'outputs': [
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_pnacl_json',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_crtbegin_o',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_ld_nexe',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_libcrt_platform_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_libgcc_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_libgcc_eh_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_libpnacl_irt_shim_a',
                          '<(PRODUCT_DIR)/pnacl/pnacl_public_x86_64_pnacl_llc_nexe',
                        ],
                        'inputs': [
                          '>(tc_lib_dir_pnacl_translate)/lib-x86-64/for_browser/libpnacl_irt_shim.a',
                        ],
                        'variables': {
                          'lib_overrides': [
                            # Use the freshly generated shim.
                            '--lib_override=x64,>(tc_lib_dir_pnacl_translate)/lib-x86-64/for_browser/libpnacl_irt_shim.a',
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
              '--pnacl_translator_path=<(SHARED_INTERMEDIATE_DIR)/<(TOOLCHAIN_OS)_x86/pnacl_translator',
              '--package_version_path=<(DEPTH)/native_client/build/package_version/package_version.py',
              '--pnacl_package_name=pnacl_newlib',
              # ABI Version Number.
              '1',
            ],
          },
        ],
      }],
    ],
  }],
}
