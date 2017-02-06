# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This GYP file defines untrusted (NaCl) targets.  All targets in this
# file should be conditionally depended upon via 'disable_nacl!=1' to avoid
# requiring NaCl sources for building.

{
  'includes': [
    '../build/common_untrusted.gypi',
    'ppapi_sources.gypi',
  ],
  'targets': [
    {
      'target_name': 'nacl_elf_loader',
      'type': 'none',
      'conditions': [
        ['target_arch=="arm" and disable_glibc==0', {
          'dependencies': [
            '<(DEPTH)/native_client/src/untrusted/elf_loader/elf_loader.gyp:elf_loader_nexe',
          ],
          'actions': [
          {
            'action_name': 'copy_arm_elf_loader',
            'message': 'Copying elf_loader_arm.nexe',
            'inputs': [
              '<(PRODUCT_DIR)/elf_loader_newlib_arm.nexe',
            ],
            'outputs': [
              '>(tc_lib_dir_glibc_arm)/elf_loader_arm.nexe',
            ],
            'action': [
              'python', '<(DEPTH)/build/cp.py', '>@(_inputs)', '>@(_outputs)'
            ],
          }],
        }],
      ],
    },
    {
      'target_name': 'ppapi_cpp_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libppapi_cpp.a',
        'nso_target': 'libppapi_cpp.so',
        'nacl_untrusted_build': 1,
        'build_glibc': 1,
        'build_newlib': 1,
        'build_pnacl_newlib': 1,
        'sources': [
          '<@(cpp_source_files)',
          'cpp/module_embedder.h',
          'cpp/ppp_entrypoints.cc',
        ],
      },
    },
    {
      'target_name': 'ppapi_gles2_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libppapi_gles2.a',
        'nacl_untrusted_build': 1,
        'nso_target': 'libppapi_gles2.so',
        'build_glibc': 1,
        'build_newlib': 1,
        'build_pnacl_newlib': 1,
        'include_dirs': [
          'lib/gl/include',
        ],
        'sources': [
          'lib/gl/gles2/gl2ext_ppapi.c',
          'lib/gl/gles2/gl2ext_ppapi.h',
          'lib/gl/gles2/gles2.c',
        ],
      },
    },
    {
      'target_name': 'ppapi_nacl_tests',
      'type': 'none',
      'dependencies': [
         '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:nacl_lib',
         '<(DEPTH)/native_client/src/untrusted/pthread/pthread.gyp:pthread_lib',
         'ppapi_cpp_lib',
         'nacl_elf_loader',
         'native_client/native_client.gyp:ppapi_lib',
      ],
      'variables': {
        # This is user code (vs IRT code), so tls accesses do not
        # need to be indirect through a function call.
        'newlib_tls_flags=': [],
        # TODO(bradnelson): Remove this compile flag once new nacl_rev is
        # above 9362.
        'compile_flags': [
          '-DGL_GLEXT_PROTOTYPES',
        ],
        # Speed up pnacl linking by not generating debug info for tests.
        # We compile with --strip-all under extra_args so debug info is
        # discarded anyway.  Remove this and the --strip-all flag if
        # debug info is really needed.
        'compile_flags!': [
          '-g',
        ],
        'defines': [
          'GL_GLEXT_PROTOTYPES',
        ],
        'nexe_target': 'ppapi_nacl_tests',
        'nacl_untrusted_build': 1,
        'build_newlib': 1,
        'include_dirs': [
          'lib/gl/include',
          '..',
        ],
        'link_flags': [
          '-lppapi_cpp',
          '-lppapi',
          '-pthread',
        ],
        'link_flags!': [
          '-O3',
        ],
        'translate_flags': [
          '-O0',
        ],
        'conditions': [
          ['target_arch=="ia32"', {
            'extra_deps_newlib32': [
              '>(tc_lib_dir_newlib32)/libppapi_cpp.a',
              '>(tc_lib_dir_newlib32)/libppapi.a',
            ],
            'extra_deps_glibc32': [
              '>(tc_lib_dir_glibc32)/libppapi_cpp.so',
              '>(tc_lib_dir_glibc32)/libppapi.so',
            ],
          }],
          ['target_arch=="x64" or (target_arch=="ia32" and OS=="win")', {
            'extra_deps_newlib64': [
              '>(tc_lib_dir_newlib64)/libppapi_cpp.a',
              '>(tc_lib_dir_newlib64)/libppapi.a',
            ],
            'extra_deps_glibc64': [
              '>(tc_lib_dir_glibc64)/libppapi_cpp.so',
              '>(tc_lib_dir_glibc64)/libppapi.so',
            ],
          }],
          ['target_arch=="arm"', {
            'extra_deps_arm': [
              '>(tc_lib_dir_newlib_arm)/libppapi_cpp.a',
              '>(tc_lib_dir_newlib_arm)/libppapi.a',
            ],
          }],
        ],
        'extra_deps_pnacl_newlib': [
          '>(tc_lib_dir_pnacl_newlib)/libppapi_cpp.a',
          '>(tc_lib_dir_pnacl_newlib)/libppapi.a',
        ],
        'sources': [
          '<@(test_common_source_files)',
          '<@(test_nacl_source_files)',
        ],
        'extra_args': [
          '--strip-all',
        ],
        'variables': {
          'conditions': [
            ['target_arch=="arm"', {
              'objdump': '>(nacl_glibc_tc_root)/bin/arm-nacl-objdump'
            }, {
              'objdump': '>(nacl_glibc_tc_root)/bin/x86_64-nacl-objdump'
            }],
          ]
        },
        'create_nmf': '<(DEPTH)/native_client_sdk/src/tools/create_nmf.py',
        'create_nmf_flags': [
          '--no-default-libpath',
          '--objdump=<(objdump)',
        ],
        'create_nonsfi_test_nmf': 'tests/create_nonsfi_test_nmf.py',
      },
      'conditions': [
        ['target_arch!="mipsel" and disable_glibc==0', {
          'variables': {
            'build_glibc': 1,
            # NOTE: Use /lib, not /lib64 here; it is a symbolic link which
            # doesn't work on Windows.
            'libdir_glibc64': '>(nacl_glibc_tc_root)/x86_64-nacl/lib',
            'libdir_glibc32': '>(nacl_glibc_tc_root)/x86_64-nacl/lib32',
            'nmf_glibc%': '<(PRODUCT_DIR)/>(nexe_target)_glibc.nmf',
          },
          'actions': [
          {
            'action_name': 'Generate GLIBC NMF and copy libs',
            # NOTE: create_nmf must be first, it is the script python executes
            # below.
            'inputs': ['>(create_nmf)'],
            # NOTE: There is no explicit dependency for the lib32
            # and lib64 directories created in the PRODUCT_DIR.
            # They are created as a side-effect of NMF creation.
            'outputs': ['>(nmf_glibc)'],
            'action': [
              'python',
              '>@(_inputs)',
              '>@(create_nmf_flags)',
              '--output=>(nmf_glibc)',
              '--stage-dependencies=<(PRODUCT_DIR)',
            ],
            'conditions': [
              ['target_arch=="ia32"', {
                'action': [
                  '--library-path=>(libdir_glibc32)',
                  '--library-path=>(tc_lib_dir_glibc32)',
                ],
                'inputs': ['>(out_glibc32)'],
              }],
              ['target_arch=="arm"', {
                'action': [
                  '--library-path=>(nacl_glibc_tc_root)/arm-nacl/lib',
                  '--library-path=>(tc_lib_dir_glibc_arm)',
                ],
                'inputs': ['>(out_glibc_arm)'],
              }],
              ['target_arch=="x64" or (target_arch=="ia32" and OS=="win")', {
                'action': [
                  '--library-path=>(libdir_glibc64)',
                  '--library-path=>(tc_lib_dir_glibc64)',
                ],
                'inputs': ['>(out_glibc64)'],
              }],
            ],
          },
        ],
        }],
        # Test PNaCl pre-translated code (pre-translated to save bot time).
        # We only care about testing that code generation is correct,
        # and in-browser translation is tested elsewhere.
        # NOTE: native_client/build/untrusted.gypi dictates that
        # PNaCl only generate x86-32 and x86-64 on x86 platforms,
        # ARM on ARM platforms, or MIPS on MIPS platforms, not all
        # versions always.
        # The same goes for the PNaCl shims. So, we have two variations here.
        ['disable_pnacl==0 and (target_arch=="ia32" or target_arch=="x64")', {
          'variables': {
            'build_pnacl_newlib': 1,
            'translate_pexe_with_build': 1,
            'nmf_pnacl%': '<(PRODUCT_DIR)/>(nexe_target)_pnacl.nmf',
          },
          # Shim is a dependency for the nexe because we pre-translate.
          'dependencies': [
            '<(DEPTH)/ppapi/native_client/src/untrusted/pnacl_irt_shim/pnacl_irt_shim.gyp:aot',
          ],
          'actions': [
            {
              'action_name': 'Generate PNACL NEWLIB NMF',
              # NOTE: create_nmf must be first, it is the script python executes
              # below.
              'inputs': [
                '>(create_nmf)',
              ],
              'outputs': ['>(nmf_pnacl)'],
              'action': [
                'python',
                '>@(_inputs)',
                '>@(create_nmf_flags)',
                '--output=>(nmf_pnacl)',
              ],
              'conditions': [
                ['target_arch=="ia32"', {
                  'inputs': [
                    '>(out_pnacl_newlib_x86_32_nexe)',
                  ],
                }],
                ['target_arch=="x64" or (target_arch=="ia32" and OS=="win")', {
                  'inputs': [
                    '>(out_pnacl_newlib_x86_64_nexe)',
                  ],
                }],
              ],
            },
          ],
        }],
        ['disable_pnacl==0 and (target_arch=="ia32" or target_arch=="x64" or target_arch=="arm") and OS=="linux"', {
          # In addition to above configuration, build x86-32 and arm nonsfi
          # .nexe files by translating from .pexe binary, for non-SFI mode PPAPI
          # testing.
          'variables': {
            'translate_pexe_with_build': 1,
            'nmf_nonsfi%': '<(PRODUCT_DIR)/>(nexe_target)_pnacl_nonsfi.nmf',
          },
          'conditions': [
            ['target_arch=="ia32" or target_arch=="x64"', {
              'variables': {
                'enable_x86_32_nonsfi': 1,
              },
            }],
            ['target_arch=="arm"', {
              'variables': {
                'enable_arm_nonsfi': 1,
              },
            }],
          ],
          # Shim is a dependency for the nexe because we pre-translate.
         'dependencies': [
            '<(DEPTH)/ppapi/native_client/src/untrusted/pnacl_irt_shim/pnacl_irt_shim.gyp:aot',
          ],
          'actions': [
            {
              'action_name': 'Generate PNACL NEWLIB NONSFI NMF',
              'inputs': ['>(create_nonsfi_test_nmf)'],
              'outputs': ['>(nmf_nonsfi)'],
              'action': [
                'python',
                '>(create_nonsfi_test_nmf)',
                '--output=>(nmf_nonsfi)',
              ],
              'target_conditions': [
                ['enable_x86_32_nonsfi==1 and "<(target_arch)"=="ia32"', {
                  'inputs': ['>(out_pnacl_newlib_x86_32_nonsfi_nexe)'],
                  'action': [
                    '--program=>(out_pnacl_newlib_x86_32_nonsfi_nexe)',
                    '--arch=x86-32',
                  ]
                }],
                ['enable_x86_32_nonsfi==1 and "<(target_arch)"=="x64"', {
                  'inputs': ['>(out_pnacl_newlib_x86_32_nonsfi_nexe)'],
                  'action': [
                    '--program=>(out_pnacl_newlib_x86_32_nonsfi_nexe)',
                    # This should be used only for nacl_helper_nonsfi test.
                    # In theory this should be x86-32. However, currently
                    # fallback logic to x86-32-nonsfi is not implemented,
                    # and, moreover, it would break the tests for current
                    # nacl_helper in Non-SFI mode on x64 Chrome.
                    # So, here we introduce the hack to use "x86-64" in order
                    # to take the benefit to run nacl_helper_nonsfi tests on
                    # x64 Chrome.
                    # TODO(hidehiko): Remove this hack.
                    '--arch=x86-64',
                  ]
                }],
                ['enable_arm_nonsfi==1', {
                  'inputs': ['>(out_pnacl_newlib_arm_nonsfi_nexe)'],
                  'action': [
                    '--program=>(out_pnacl_newlib_arm_nonsfi_nexe)',
                    '--arch=arm',
                  ]
                }],
              ],
            },
          ],
        }],
        ['disable_pnacl==0 and target_arch=="arm"', {
          'variables': {
            'build_pnacl_newlib': 1,
            'translate_pexe_with_build': 1,
            'nmf_pnacl%': '<(PRODUCT_DIR)/>(nexe_target)_pnacl.nmf',
          },
          # Shim is a dependency for the nexe because we pre-translate.
          'dependencies': [
            '<(DEPTH)/ppapi/native_client/src/untrusted/pnacl_irt_shim/pnacl_irt_shim.gyp:aot',
          ],
          'actions': [
            {
              'action_name': 'Generate PNACL NEWLIB NMF',
              # NOTE: create_nmf must be first, it is the script python executes
              # below.
              'inputs': ['>(create_nmf)', '>(out_pnacl_newlib_arm_nexe)'],
              'outputs': ['>(nmf_pnacl)'],
              'action': [
                'python',
                '>@(_inputs)',
                '>@(create_nmf_flags)',
                '--output=>(nmf_pnacl)',
              ],
            },
          ],
        }],
        ['disable_pnacl==0 and target_arch=="mipsel"', {
          'variables': {
            'build_pnacl_newlib': 1,
            'translate_pexe_with_build': 1,
            'nmf_pnacl%': '<(PRODUCT_DIR)/>(nexe_target)_pnacl.nmf',
          },
          # Shim is a dependency for the nexe because we pre-translate.
          'dependencies': [
            '<(DEPTH)/ppapi/native_client/src/untrusted/pnacl_irt_shim/pnacl_irt_shim.gyp:aot',
          ],
          'actions': [
            {
              'action_name': 'Generate PNACL NEWLIB NMF',
              'inputs': ['>(create_nmf)', '>(out_pnacl_newlib_mips_nexe)'],
              'outputs': ['>(nmf_pnacl)'],
              'action': [
                'python',
                '>@(_inputs)',
                '>@(create_nmf_flags)',
                '--output=>(nmf_pnacl)',
              ],
            },
          ],
        }],
      ],
    },
  ],
}
