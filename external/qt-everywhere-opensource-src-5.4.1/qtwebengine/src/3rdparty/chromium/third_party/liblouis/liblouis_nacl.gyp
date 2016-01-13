# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'braille_test_data_dir': '<(PRODUCT_DIR)/chromevox_test_data/braille',
    'braille_chromevox_dir': '<(PRODUCT_DIR)/resources/chromeos/chromevox/chromevox/background/braille',
    'table_files': [
      '>!@pymod_do_main(liblouis_list_tables -D overrides/tables -D src/tables tables.json)',
    ],
  },
  # x86 targets build both 32 and 64 bit binaries by default.  We only need
  # the one that matches our target architecture.
  'target_defaults': {
    'conditions': [
      ['target_arch=="ia32"', {
        'variables': {
          'enable_x86_64': 0,
        },
      }],
      ['target_arch=="x64"', {
        'variables': {
          'enable_x86_32': 0,
        },
      }],
    ],
  },
  'includes': [
    '../../build/common_untrusted.gypi',
  ],
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'liblouis_nacl',
          'type': 'none',
          'variables': {
            'nacl_untrusted_build': 1,
            'nlib_target': 'liblouis_nacl.a',
            'build_newlib': 1,
          },
          'compile_flags': [
            '-Wno-switch',
            '-Wno-unused-but-set-variable',
          ],
          'include_dirs': [
            'overrides/liblouis',
            'src/liblouis',
            '.',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'overrides',
            ],
          },
          'sources': [
            'overrides/liblouis/config.h',
            'overrides/liblouis/liblouis.h',
            'overrides/liblouis/compileTranslationTable.c',
            'src/liblouis/lou_backTranslateString.c',
            'src/liblouis/lou_translateString.c',
            'src/liblouis/transcommon.ci',
            'src/liblouis/wrappers.c',
          ],
          'dependencies': [
            '../../native_client/tools.gyp:prep_toolchain',
          ],
        },
        {
          'target_name': 'liblouis_nacl_wrapper_nacl',
          'type': 'none',
          'variables': {
            'nacl_untrusted_build': 1,
            'nexe_target': 'liblouis_nacl',
            'out_newlib64': '<(braille_test_data_dir)/>(nexe_target)_x86_64.nexe',
            'out_newlib32': '<(braille_test_data_dir)/>(nexe_target)_x86_32.nexe',
            'out_newlib_arm': '<(braille_test_data_dir)/>(nexe_target)_arm.nexe',
            'out_newlib_mips': '<(braille_test_data_dir)/>(nexe_target)_mips32.nexe',
            'build_newlib': 1,
            'extra_args': [
              '--strip-debug',
            ],
            'nmf': '<(braille_test_data_dir)/>(nexe_target).nmf',
            'target_conditions': [
              ['enable_x86_64==1', {
                'nexe_files': ['>(out_newlib64)'],
              }],
              ['enable_x86_32==1', {
                'nexe_files': ['>(out_newlib32)'],
              }],
              ['enable_arm==1', {
                'nexe_files': ['>(out_newlib_arm)'],
              }],
              ['enable_mips==1', {
                'nexe_files': ['>(out_newlib_mips)'],
              }],
            ],
          },
          'sources': [
            'nacl_wrapper/liblouis_instance.h',
            'nacl_wrapper/liblouis_instance.cc',
            'nacl_wrapper/liblouis_module.h',
            'nacl_wrapper/liblouis_module.cc',
            'nacl_wrapper/liblouis_wrapper.h',
            'nacl_wrapper/liblouis_wrapper.cc',
            'nacl_wrapper/translation_params.h',
            'nacl_wrapper/translation_result.h',
          ],
          'link_flags': [
            '-lppapi',
            '-lppapi_cpp',
            '-llouis_nacl',
            '-ljsoncpp_nacl',
            '-lpthread',
            '-lnacl_io',
          ],
          'dependencies': [
            '../../native_client/src/untrusted/nacl/nacl.gyp:nacl_lib',
            '../../native_client/tools.gyp:prep_toolchain',
            '../../native_client_sdk/native_client_sdk_untrusted.gyp:nacl_io_untrusted',
            '../../ppapi/native_client/native_client.gyp:ppapi_lib',
            '../../ppapi/ppapi_nacl.gyp:ppapi_cpp_lib',
            '../jsoncpp/jsoncpp_nacl.gyp:jsoncpp_nacl',
            'liblouis_nacl',
          ],
          'actions': [
            {
              'action_name': 'Generate NEWLIB NMF',
              'inputs': [
                '>@(nexe_files)',
              ],
              'outputs': ['>(nmf)'],
              'action': [
                'python',
                '<(DEPTH)/native_client_sdk/src/tools/create_nmf.py',
                '>@(_inputs)',
                '--output=>(nmf)',
              ],
            },
          ],
          # Copy specific files into the product directory to avoid
          # copying over the unstripped binary file.
          'copies': [
            {
              'destination': '<(braille_chromevox_dir)',
              'files': [
                '<(nmf)',
                '>@(nexe_files)',
                'tables.json',
              ],
            },
            {
              'destination': '<(braille_chromevox_dir)/tables',
              'files': [
                '<@(table_files)',
              ],
            },
          ],
        },
        {
          'target_name': 'liblouis_test_data',
          'type': 'none',
          'variables': {
            'test_extension_dir': '<(DEPTH)/chrome/test/data/chromeos/liblouis_nacl',
          },
          'dependencies': [
            'liblouis_nacl_wrapper_nacl',
          ],
          'copies': [
            {
              'destination': '<(braille_test_data_dir)',
              'files': [
                'tables.json',
                '<(test_extension_dir)/manifest.json',
                '<(test_extension_dir)/test.js',
              ],
            },
            {
              'destination': '<(braille_test_data_dir)/tables',
              'files': [
                '<@(table_files)',
              ],
            },
          ],
        },
      ],
    }],
  ],
}
