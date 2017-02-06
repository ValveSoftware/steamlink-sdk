# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'variables': {
      # Enable -Werror by default, but put it in a variable so it can
      # be optionally disabled.
      'werror%': '-Werror',
      # 1 to use goma.
      'use_goma%': 0,
    },
    'common_args': [
      'python',
      '<(DEPTH)/native_client/build/build_nexe.py',
      '--root', '<(DEPTH)',
      # Horrible hack: postfix the product-dir with a fake child.
      # Without this hack, MSVS will expand the PRODUCT_DIR variable to a path
      # with a trailing backslash, and that backslash will escape the rightmost
      # quote around the path causing serious problems parsing the resulting
      # command line. The fake child prevents an accidental escape while also
      # being robust against GYP's attempt to normalize the path.
      # Technically this hack is only needed for Windows, but conditional logic
      # would just make things uglier.
      '--product-dir', '<(PRODUCT_DIR)/xyz',
      '--config-name', '<(CONFIGURATION_NAME)',
      '-t', '<(DEPTH)/native_client/toolchain/',
    ],
    'common_inputs': [
      '<(DEPTH)/native_client/build/build_nexe.py',
      '<(DEPTH)/native_client/build/build_nexe_tools.py',
    ],
    # Default C compiler defines.
    'nacl_default_defines': [
      '__STDC_LIMIT_MACROS=1',
      '__STDC_FORMAT_MACROS=1',
      '_GNU_SOURCE=1',
      '_POSIX_C_SOURCE=199506',
      '_XOPEN_SOURCE=600',
      'DYNAMIC_ANNOTATIONS_ENABLED=1',
      'DYNAMIC_ANNOTATIONS_PREFIX=NACL_',
    ],
    'nacl_default_compile_flags': [
      #'-std=gnu99',  Added by build_nexe
      '-O2',
      '-g',
      '-Wall',
      '-fdiagnostics-show-option',
      '<(werror)',
    ],
    # The flag gomadir is one of the flag to enable goma on build_nexe.py.
    # It should be set only if use_goma is true.
    'conditions': [
      ['use_goma==0', {
        'gomadir%': '',
      }],
      ['OS=="android"', {
        'TOOLCHAIN_OS': 'linux',
      }, {
        'TOOLCHAIN_OS': '<(OS)',
      }],
    ],
  },
  'conditions': [
    ['target_arch=="ia32" or target_arch=="x64"', {
      # Common defaults for all x86 nacl-gcc targets
      'target_defaults': {
        'defines': [],
        'sources': [],
        'compile_flags': [],
        'link_flags': [],
        'include_dirs': [],
        'variables': {
          'nexe_target': '',
          'nlib_target': '',
          'nso_target': '',
          'build_newlib': 0,
          'build_glibc': 0,
          'build_irt': 0,
          'build_nonsfi_helper': 0,
          'disable_glibc%': 0,
          'disable_newlib%': 0,
          'extra_args': [],
          'enable_x86_32': 1,
          'enable_x86_64': 1,
          'enable_arm': 0,
          'enable_mips': 0,
          'enable_x86_32_nonsfi': 0,
          'enable_arm_nonsfi': 0,
          'nacl_glibc_tc_root': '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/nacl_x86_glibc',
          'tc_lib_dir_newlib32': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32',
          'tc_lib_dir_newlib64': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64',
          'tc_lib_dir_glibc32': '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32',
          'tc_lib_dir_glibc64': '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64',
          'tc_lib_dir_irt32': '<(SHARED_INTERMEDIATE_DIR)/tc_irt/lib32',
          'tc_lib_dir_irt64': '<(SHARED_INTERMEDIATE_DIR)/tc_irt/lib64',
          'tc_lib_dir_nonsfi_helper32': '<(SHARED_INTERMEDIATE_DIR)/tc_nonsfi_helper/lib32',
          'tc_include_dir_newlib': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/include',
          'tc_include_dir_glibc': '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/include',
          'extra_deps': [],
          'extra_deps_newlib64': [],
          'extra_deps_newlib32': [],
          'extra_deps_glibc64': [],
          'extra_deps_glibc32': [],
          'extra_deps_newlib32_nonsfi': [],
          'include_dirs': ['<(DEPTH)'],
          'defines': [
            '<@(nacl_default_defines)',
            'NACL_BUILD_ARCH=x86',
          ],
          'sources': [],
          'link_flags': [],
          # X86-32 IRT needs to be callable with an under-aligned stack so we
          # disable SSE instructions, which can fault on misaligned addresses:
          # see https://code.google.com/p/nativeclient/issues/detail?id=3935
          'irt_flags_x86_32': '-mstackrealign -mno-sse',
        },
      },
    }],
    ['target_arch=="arm"', {
      # Common defaults for all ARM nacl-gcc targets
      'target_defaults': {
        'defines': [],
        'sources': [],
        'compile_flags': [],
        'link_flags': [],
        'include_dirs': [],
        'variables': {
          'nexe_target': '',
          'nlib_target': '',
          'nso_target': '',
          'build_newlib': 0,
          'build_glibc': 0,
          'build_irt': 0,
          'build_nonsfi_helper': 0,
          'disable_newlib%': 0,
          'disable_glibc%': 0,
          'extra_args': [],
          'enable_x86_32': 0,
          'enable_x86_64': 0,
          'enable_arm': 1,
          'enable_mips': 0,
          'enable_x86_32_nonsfi': 0,
          'enable_arm_nonsfi': 0,
          'extra_deps': [],
          'extra_deps_newlib_arm': [],
          'extra_deps_newlib_arm_nonsfi': [],
          'native_sources': [],
          'nacl_glibc_tc_root': '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/nacl_arm_glibc',
          'tc_lib_dir_newlib_arm': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm',
          'tc_lib_dir_glibc_arm': '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/libarm',
          'tc_lib_dir_irt_arm': '<(SHARED_INTERMEDIATE_DIR)/tc_irt/libarm',
          'tc_lib_dir_nonsfi_helper_arm': '<(SHARED_INTERMEDIATE_DIR)/tc_nonsfi_helper/libarm',
          'tc_include_dir_newlib': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/include',
          'tc_include_dir_glibc': '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/include',
          'include_dirs': ['<(DEPTH)'],
          'defines': [
            '<@(nacl_default_defines)',
            'NACL_BUILD_ARCH=arm',
           ],
           'sources': [],
           'link_flags': [],
         },
       },
    }],
    ['target_arch=="mipsel"', {
      # Common defaults for all mips pnacl-clang targets
      'target_defaults': {
        'defines': [],
        'sources': [],
        'compile_flags': [],
        'link_flags': [],
        'include_dirs': [],
        'variables': {
          'nexe_target': '',
          'nlib_target': '',
          'nso_target': '',
          'build_newlib': 0,
          'build_glibc': 0,
          'build_irt': 0,
          'build_nonsfi_helper': 0,
          'disable_newlib%': 0,
          'disable_glibc%': 1,
          'extra_args': [],
          'enable_x86_32': 0,
          'enable_x86_64': 0,
          'enable_arm': 0,
          'enable_mips': 1,
          'enable_x86_32_nonsfi': 0,
          'enable_arm_nonsfi': 0,
          'extra_deps': [],
          'extra_deps_newlib_mips': [],
          'native_sources': [],
          'nacl_glibc_tc_root': '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/nacl_mips_glibc',
          'tc_lib_dir_newlib_mips': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libmips',
          'tc_lib_dir_irt_mips': '<(SHARED_INTERMEDIATE_DIR)/tc_irt/libmips',
          'tc_include_dir_newlib': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/include',
          'include_dirs': ['<(DEPTH)'],
          'defines': [
            '<@(nacl_default_defines)',
            'NACL_BUILD_ARCH=mips',
           ],
           'sources': [],
           'link_flags': [],
         },
       },
    }],
    ['target_arch=="ia32" or target_arch=="x64"', {
      'target_defaults': {
        # x86-64 newlib nexe action
        'target_conditions': [
           ['nexe_target!="" and build_newlib!=0 and disable_newlib==0 and enable_x86_64!=0', {
             'variables': {
                'tool_name': 'newlib',
                'out_newlib64%': '<(PRODUCT_DIR)/>(nexe_target)_newlib_x64.nexe',
                'objdir_newlib64%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-64/>(_target_name)',
             },
             'actions': [
               {
                 'action_name': 'build newlib x86-64 nexe',
                 'variables': {
                   'source_list_newlib64%': '^|(<(tool_name)-x86-64.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_newlib64)',
                 'inputs': [
                    '<@(common_inputs)',
                    '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                    '>@(extra_deps)',
                    '>@(extra_deps_newlib64)',
                    '^(source_list_newlib64)',
                    '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json',
                 ],
                 'outputs': ['>(out_newlib64)'],
                 'action': [
                   '<@(common_args)',
                   '>@(extra_args)',
                   '--arch', 'x86-64',
                   '--build', 'newlib_nexe_clang',
                   '--name', '>(out_newlib64)',
                   '--objdir', '>(objdir_newlib64)',
                   '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=-m64 ^(compile_flags) >(_compile_flags)',
                   '--gomadir', '<(gomadir)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-B>(tc_lib_dir_newlib64) -L>(tc_lib_dir_newlib64) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_newlib64)',
                 ],
               },
             ],
           }],
           # x86-64 newlib library action
           ['nlib_target!="" and build_newlib!=0 and disable_newlib==0 and enable_x86_64!=0', {
             'variables': {
                'tool_name': 'newlib',
                'objdir_newlib64%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-64/>(_target_name)',
                'out_newlib64%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib64/>(nlib_target)',
             },
             'actions': [
               {
                 'action_name': 'build newlib x86-64 nlib',
                 'variables': {
                   'source_list_newlib64%': '^|(<(tool_name)-x86-64.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_newlib64)',
                 'inputs': [
                    '<@(common_inputs)',
                    '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                    '>@(extra_deps)',
                    '>@(extra_deps_newlib64)',
                    '^(source_list_newlib64)',
                    '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json',
                 ],
                 'outputs': ['>(out_newlib64)'],
                 'action': [
                   '<@(common_args)',
                   '>@(extra_args)',
                   '--arch', 'x86-64',
                   '--build', 'newlib_nlib_clang',
                   '--name', '>(out_newlib64)',
                   '--objdir', '>(objdir_newlib64)',
                   '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=-m64 ^(compile_flags) >(_compile_flags)',
                   '--gomadir', '<(gomadir)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-B>(tc_lib_dir_newlib64) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_newlib64)',
                 ],
               },
             ],
           }],
           # x86-64 IRT nexe action
           ['nexe_target!="" and build_irt!=0 and enable_x86_64!=0', {
             'variables': {
                'tool_name': 'irt',
                'out_newlib64%': '<(PRODUCT_DIR)/>(nexe_target)_newlib_x64.nexe',
                'objdir_newlib64%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-64/>(_target_name)',
             },
             'actions': [
               {
                 'action_name': 'build IRT x86-64 nexe',
                 'variables': {
                   'source_list_newlib64%': '^|(<(tool_name)-x86-64.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_newlib64)',
                 'inputs': [
                    '<@(common_inputs)',
                    '<(DEPTH)/native_client/build/link_irt.py',
                    '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                    '>@(extra_deps)',
                    '>@(extra_deps_newlib64)',
                    '^(source_list_newlib64)',
                    '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json',
                    '<(PRODUCT_DIR)/tls_edit<(EXECUTABLE_SUFFIX)',
                 ],
                 'outputs': ['>(out_newlib64)'],
                 'action': [
                   '<@(common_args)',
                   '>@(extra_args)',
                   '--arch', 'x86-64',
                   '--build', 'newlib_nexe_clang',
                   '--name', '>(out_newlib64)',
                   '--objdir', '>(objdir_newlib64)',
                   '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',

                   '--compile_flags=^(pnacl_compile_flags) >(_pnacl_compile_flags) ^(native_irt_compile_flags) -integrated-as',
                   '--gomadir', '<(gomadir)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-B>(tc_lib_dir_irt64) -L>(tc_lib_dir_irt64)  ^(link_flags) >(_link_flags) ^(native_irt_link_flags) >(libcpp_irt_stdlibs)',
                   '--source-list=^(source_list_newlib64)',
                   '--tls-edit=<(PRODUCT_DIR)/tls_edit<(EXECUTABLE_SUFFIX)',
                   '--irt-linker=<(DEPTH)/native_client/build/link_irt.py',
                 ],
               },
             ],
           }],
           # x86-64 IRT library action
           ['nlib_target!="" and build_irt!=0 and enable_x86_64!=0', {
             'variables': {
                'tool_name': 'irt',
                'objdir_newlib64%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-64/>(_target_name)',
                'out_newlib64%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib64/>(nlib_target)',
             },
             'actions': [
               {
                 'action_name': 'build irt x86-64 nlib',
                 'variables': {
                   'source_list_newlib64%': '^|(<(tool_name)-x86-64.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_newlib64)',
                 'inputs': [
                    '<@(common_inputs)',
                    '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                    '>@(extra_deps)',
                    '>@(extra_deps_newlib64)',
                    '^(source_list_newlib64)',
                    '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json',
                 ],
                 'outputs': ['>(out_newlib64)'],
                 'action': [
                   '<@(common_args)',
                   '>@(extra_args)',
                   '--arch', 'x86-64',
                   '--build', 'newlib_nlib_clang',
                   '--name', '>(out_newlib64)',
                   '--objdir', '>(objdir_newlib64)',
                   '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags) ^(native_irt_compile_flags) -integrated-as',
                   '--gomadir', '<(gomadir)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-B>(tc_lib_dir_irt64) ^(link_flags) >(_link_flags) ^(native_irt_link_flags) >(libcpp_irt_stdlibs)',
                   '--source-list=^(source_list_newlib64)',
                 ],
               },
             ],
           }],
           # x86-32 newlib nexe action
           ['nexe_target!="" and build_newlib!=0 and disable_newlib==0 and enable_x86_32!=0', {
             'variables': {
                'tool_name': 'newlib',
                'out_newlib32%': '<(PRODUCT_DIR)/>(nexe_target)_newlib_x32.nexe',
                'objdir_newlib32%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-32/>(_target_name)',
             },
             'actions': [
               {
                 'action_name': 'build newlib x86-32 nexe',
                 'variables': {
                   'source_list_newlib32%': '^|(<(tool_name)-x86-32.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_newlib32)',
                 'inputs': [
                    '<@(common_inputs)',
                    '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                    '>@(extra_deps)',
                    '>@(extra_deps_newlib32)',
                    '^(source_list_newlib32)',
                    '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json',
                 ],
                 'outputs': ['>(out_newlib32)'],
                 'action': [
                   '<@(common_args)',
                   '>@(extra_args)',
                   '--arch', 'x86-32',
                   '--build', 'newlib_nexe_clang',
                   '--name', '>(out_newlib32)',
                   '--objdir', '>(objdir_newlib32)',
                   '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=-m32 ^(compile_flags) >(_compile_flags)',
                   '--gomadir', '<(gomadir)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-m32 -B>(tc_lib_dir_newlib32) -L>(tc_lib_dir_newlib32) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_newlib32)',
                 ],
               },
             ],
           }],
           # x86-32 newlib library action
           ['nlib_target!="" and build_newlib!=0 and disable_newlib==0 and enable_x86_32!=0', {
             'variables': {
                'tool_name': 'newlib',
                'out_newlib32%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib32/>(nlib_target)',
                'objdir_newlib32%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-32/>(_target_name)',
             },
             'actions': [
               {
                 'action_name': 'build newlib x86-32 nlib',
                 'variables': {
                   'source_list_newlib32%': '^|(<(tool_name)-x86-32.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_newlib32)',
                 'inputs': [
                    '<@(common_inputs)',
                    '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                    '>@(extra_deps)',
                    '>@(extra_deps_newlib32)',
                    '^(source_list_newlib32)',
                    '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json',
                 ],
                 'outputs': ['>(out_newlib32)'],
                 'action': [
                   '<@(common_args)',
                   '>@(extra_args)',
                   '--arch', 'x86-32',
                   '--build', 'newlib_nlib_clang',
                   '--name', '>(out_newlib32)',
                   '--objdir', '>(objdir_newlib32)',
                   '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=-m32  ^(compile_flags) >(_compile_flags)',
                   '--gomadir', '<(gomadir)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-m32 -B>(tc_lib_dir_newlib32) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_newlib32)',
                 ],
               },
             ],
           }],
           # x86-32 IRT nexe build
           ['nexe_target!="" and build_irt!=0 and enable_x86_32!=0', {
             'variables': {
                'tool_name': 'irt',
                'out_newlib32%': '<(PRODUCT_DIR)/>(nexe_target)_newlib_x32.nexe',
                'objdir_newlib32%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-32/>(_target_name)',
             },
             'actions': [
               {
                 'action_name': 'build IRT x86-32 nexe',
                 'variables': {
                   'source_list_newlib32%': '^|(<(tool_name)-x86-32.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_newlib32)',
                 'inputs': [
                    '<@(common_inputs)',
                    '<(DEPTH)/native_client/build/link_irt.py',
                    '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                    '>@(extra_deps)',
                    '>@(extra_deps_newlib32)',
                    '^(source_list_newlib32)',
                    '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json',
                    '<(PRODUCT_DIR)/tls_edit<(EXECUTABLE_SUFFIX)',
                 ],
                 'outputs': ['>(out_newlib32)'],
                 'action': [
                   '<@(common_args)',
                   '>@(extra_args)',
                   '--arch', 'x86-32',
                   '--build', 'newlib_nexe_clang',
                   '--name', '>(out_newlib32)',
                   '--objdir', '>(objdir_newlib32)',
                   '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=-m32 ^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags) >(irt_flags_x86_32) ^(native_irt_compile_flags) -integrated-as',
                   '--gomadir', '<(gomadir)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-m32 -B>(tc_lib_dir_irt32) -L>(tc_lib_dir_irt32) ^(link_flags) >(_link_flags) ^(native_irt_link_flags) >(libcpp_irt_stdlibs)',
                   '--source-list=^(source_list_newlib32)',
                   '--tls-edit=<(PRODUCT_DIR)/tls_edit<(EXECUTABLE_SUFFIX)',
                   '--irt-linker=<(DEPTH)/native_client/build/link_irt.py',
                 ],
               },
             ],
           }],
           # x86-32 IRT library build
           ['nlib_target!="" and build_irt!=0 and enable_x86_32!=0', {
             'variables': {
                'tool_name': 'irt',
                'out_newlib32%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib32/>(nlib_target)',
                'objdir_newlib32%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-32/>(_target_name)',
             },
             'actions': [
               {
                 'action_name': 'build IRT x86-32 nlib',
                 'variables': {
                   'source_list_newlib32%': '^|(<(tool_name)-x86-32.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_newlib32)',
                 'inputs': [
                    '<@(common_inputs)',
                    '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                    '>@(extra_deps)',
                    '>@(extra_deps_newlib32)',
                    '^(source_list_newlib32)',
                    '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json',
                 ],
                 'outputs': ['>(out_newlib32)'],
                 'action': [
                   '<@(common_args)',
                   '>@(extra_args)',
                   '--arch', 'x86-32',
                   '--build', 'newlib_nlib_clang',
                   '--name', '>(out_newlib32)',
                   '--objdir', '>(objdir_newlib32)',
                   '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=-m32 >(irt_flags_x86_32) ^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags) ^(native_irt_compile_flags) -integrated-as',
                   '--gomadir', '<(gomadir)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-m32 -B>(tc_lib_dir_irt32) ^(link_flags) >(_link_flags) ^(native_irt_link_flags) >(libcpp_irt_stdlibs)',
                   '--source-list=^(source_list_newlib32)',
                 ],
               },
             ],
           }],
        ],
      },
    }], # end x86 gcc nexe/nlib actions
    ['(target_arch=="ia32" or target_arch=="x64") and OS=="linux"', {
      'target_defaults': {
        'target_conditions': [
          # x86-32 non-SFI helper nexe build.
          ['nexe_target!="" and build_nonsfi_helper!=0', {
            'variables': {
              'tool_name': 'nonsfi_helper',
              'out_newlib32_nonsfi%': '<(PRODUCT_DIR)/>(nexe_target)_newlib_x32_nonsfi.nexe',
              'objdir_newlib32_nonsfi%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-32/>(_target_name)',
            },
            'actions': [
              {
                'action_name': 'build non-SFI helper x86-32 nexe',
                'variables': {
                  'source_list_newlib32_nonsfi%': '^|(<(tool_name)-x86-32-nonsfi.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                  'stdlibs': ['-lc++', '-lm', '-lnacl', '-lc', '-lpnaclmm'],
                },
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_newlib32_nonsfi)',
                'inputs': [
                  '<@(common_inputs)',
                  '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                  '>@(extra_deps)',
                  '>@(extra_deps_newlib32_nonsfi)',
                  '^(source_list_newlib32_nonsfi)',
                  '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json',
                  '>(tc_lib_dir_nonsfi_helper32)/libnacl_sys_private.a',
                ],
                'outputs': ['>(out_newlib32_nonsfi)'],
                'action': [
                  '<@(common_args)',
                  '>@(extra_args)',
                  '--arch', 'x86-32-nonsfi',
                  '--build', 'newlib_nexe_pnacl',
                  '--name', '>(out_newlib32_nonsfi)',
                  '--objdir', '>(objdir_newlib32_nonsfi)',
                  '--include-dirs=>(tc_include_dir_newlib) <(DEPTH)/native_client/src/public/linux_syscalls ^(include_dirs) >(_include_dirs)',
                  '--compile_flags=--target=i686-unknown-nacl --pnacl-bias=x86-32-nonsfi ^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags)',
                  '--gomadir', '<(gomadir)',
                  '--defines=^(defines) >(_defines)',
                  # Both -Wl,--noirt and -Wt,--noirt are needed here. The
                  # former is to prevent linking to irt. The latter controls
                  # the entry point for Non-SFI NaCl in pnacl-translate.
                  # "-nodefaultlibs -Wl,--start-group, ... -Wl,--end-group"
                  # is the flags to exclude -lpthread, otherwise it causes
                  # library not found error. Note that pthread related code
                  # is contained in libnacl_sys_private.a, which is
                  # automatically linked.
                  # Place >@(stdlibs) early and group it with libraries from
                  # link_flags. The mixture of native libs and bitcode started
                  # having problems in LLVM 3.6, when stdlibs were grouped
                  # separately and placed later. See:
                  # https://code.google.com/p/nativeclient/issues/detail?id=4067
                  # TODO(jvoung): get to the bottom of this.
                  '--link_flags=--target=i686-unknown-nacl -arch x86-32-nonsfi --pnacl-allow-translate --pnacl-allow-native -Wl,--noirt -Wt,--noirt -Wt,--noirtshim -nodefaultlibs -B>(tc_lib_dir_nonsfi_helper32) -Wl,--start-group >@(stdlibs) ^(link_flags) >(_link_flags) -Wl,--end-group',
                  '--source-list=^(source_list_newlib32_nonsfi)',
                ],
              },
            ],
          }],

          # x86-32 non-SFI helper library build or PNaCl PPAPI shim for nonsfi
          # build.
          ['nlib_target!="" and (build_nonsfi_helper!=0 or (pnacl_native_biased!=0 and enable_x86_32_nonsfi==1))', {
            'variables': {
              'tool_name': 'nonsfi_helper',
              'out_newlib32_nonsfi%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib32/>(nlib_target)',
              'objdir_newlib32_nonsfi%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-32-nonsfi/>(_target_name)',
            },
            'actions': [
              {
                'action_name': 'build nonsfi_helper x86-32 nlib',
                'variables': {
                  'source_list_newlib32_nonsfi%': '^|(<(tool_name)-x86-32-nonsfi.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                },
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_newlib32_nonsfi)',
                'inputs': [
                  '<@(common_inputs)',
                  '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                  '>@(extra_deps)',
                  '>@(extra_deps_newlib32_nonsfi)',
                  '^(source_list_newlib32_nonsfi)',
                  '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json',
                ],
                'outputs': ['>(out_newlib32_nonsfi)'],
                'action': [
                  '<@(common_args)',
                  '>@(extra_args)',
                  '--arch', 'x86-32-nonsfi',
                  '--build', 'newlib_nlib_pnacl',
                  '--name', '>(out_newlib32_nonsfi)',
                  '--objdir', '>(objdir_newlib32_nonsfi)',
                  '--include-dirs=>(tc_include_dir_newlib) <(DEPTH)/native_client/src/public/linux_syscalls ^(include_dirs) >(_include_dirs)',
                  '--compile_flags=--target=i686-unknown-nacl --pnacl-bias=x86-32-nonsfi --pnacl-allow-translate --pnacl-allow-native -arch x86-32-nonsfi ^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags)',
                  '--gomadir', '<(gomadir)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-B>(tc_lib_dir_nonsfi_helper32) ^(link_flags) >(_link_flags)',
                  '--source-list=^(source_list_newlib32_nonsfi)',
                ],
              },
            ],
          }],
        ],
      },
    }],  # end x86-32 Non-SFI helper nexe / library actions.
    ['target_arch=="arm"', {
      'target_defaults': {
        'target_conditions': [
          # arm glibc nexe action
          ['nexe_target!="" and build_glibc!=0 and disable_glibc==0', {
            'variables': {
               'tool_name': 'glibc',
               'out_glibc_arm%': '<(PRODUCT_DIR)/>(nexe_target)_glibc_arm.nexe',
               'objdir_glibc_arm%': '>(INTERMEDIATE_DIR)/<(tool_name)-arm/>(_target_name)',
            },
            'actions': [
              {
                'action_name': 'build glibc arm nexe',
                'variables': {
                  'source_list_glibc_arm%': '^|(<(tool_name)-arm.>(_target_name).source_list.gypcmd ^(_sources) ^(sources) ^(native_sources))',
                },
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_glibc_arm)',
                'inputs': [
                   '<@(common_inputs)',
                   '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources) >(native_sources))',
                   '>@(extra_deps)',
                   '^(source_list_glibc_arm)',
                   '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/nacl_arm_glibc/nacl_arm_glibc.json',
                ],
                'outputs': ['>(out_glibc_arm)'],
                'action': [
                  '<@(common_args)',
                  '>@(extra_args)',
                  '--arch', 'arm',
                  '--build', 'glibc_nexe',
                  '--name', '>(out_glibc_arm)',
                  '--objdir', '>(objdir_glibc_arm)',
                  '--include-dirs=>(tc_include_dir_glibc) ^(include_dirs) >(_include_dirs)',
                  '--compile_flags=^(compile_flags) >(_compile_flags)',
                  '--gomadir', '<(gomadir)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-B>(tc_lib_dir_glibc_arm) -L>(tc_lib_dir_glibc_arm) ^(link_flags) >(_link_flags)',
                  '--source-list=^(source_list_glibc_arm)',
                ],
              },
            ],
          }],
          # arm glibc static library action
          ['nlib_target!="" and build_glibc!=0 and disable_glibc==0', {
            'variables': {
              'tool_name': 'glibc',
              'out_glibc_arm%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/libarm/>(nlib_target)',
              'objdir_glibc_arm%': '>(INTERMEDIATE_DIR)/<(tool_name)-arm/>(_target_name)',
            },
            'actions': [
              {
                'action_name': 'build glibc arm nlib',
                 'variables': {
                   'source_list_glibc_arm%': '^|(<(tool_name)-arm.>(_target_name).source_list.gypcmd ^(_sources) ^(sources) ^(native_sources))',
                 },
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_glibc_arm)',
                'inputs': [
                   '<@(common_inputs)',
                   '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources) >(native_sources))',
                   '>@(extra_deps)',
                   '^(source_list_glibc_arm)',
                   '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/nacl_arm_glibc/nacl_arm_glibc.json',
                ],
                'outputs': ['>(out_glibc_arm)'],
                'action': [
                  '<@(common_args)',
                  '>@(extra_args)',
                  '--arch', 'arm',
                  '--build', 'glibc_nlib',
                  '--name', '>(out_glibc_arm)',
                  '--objdir', '>(objdir_glibc_arm)',
                  '--include-dirs=>(tc_include_dir_glibc) ^(include_dirs) >(_include_dirs)',
                  '--compile_flags=^(compile_flags) >(_compile_flags)',
                  '--gomadir', '<(gomadir)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-B>(tc_lib_dir_glibc_arm) ^(link_flags) >(_link_flags)',
                  '--source-list=^(source_list_glibc_arm)',
                ],
              },
            ],
          }],
          # arm glibc shared library action
          ['nso_target!="" and build_glibc!=0 and disable_glibc==0', {
             'variables': {
                'tool_name': 'glibc',
                'out_glibc_arm%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/libarm/>(nso_target)',
                'objdir_glibc_arm%': '>(INTERMEDIATE_DIR)/<(tool_name)-arm-so/>(_target_name)',
             },
             'actions': [
               {
                 'action_name': 'build glibc arm nso',
                 'variables': {
                   'source_list_glibc_arm%': '^|(<(tool_name)-arm-so.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_glibc_arm)',
                 'inputs': [
                    '<@(common_inputs)',
                    '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                    '>@(extra_deps)',
                    '^(source_list_glibc_arm)',
                    '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/nacl_arm_glibc/nacl_arm_glibc.json',
                 ],
                 'outputs': ['>(out_glibc_arm)'],
                 'action': [
                   '<@(common_args)',
                   '>@(extra_args)',
                   '--arch', 'arm',
                   '--build', 'glibc_nso',
                   '--name', '>(out_glibc_arm)',
                   '--objdir', '>(objdir_glibc_arm)',
                   '--include-dirs=>(tc_include_dir_glibc) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=-fPIC ^(compile_flags) >(_compile_flags)',
                   '--gomadir', '<(gomadir)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-B>(tc_lib_dir_glibc_arm) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_glibc_arm)',
                 ],
               },
             ],
           }],
          # arm newlib nexe action
          ['nexe_target!="" and build_newlib!=0 and disable_newlib==0', {
            'variables': {
               'tool_name': 'newlib',
               'out_newlib_arm%': '<(PRODUCT_DIR)/>(nexe_target)_newlib_arm.nexe',
               'objdir_newlib_arm%': '>(INTERMEDIATE_DIR)/<(tool_name)-arm/>(_target_name)',
            },
            'actions': [
              {
                'action_name': 'build newlib arm nexe',
                'variables': {
                  'source_list_newlib_arm%': '^|(<(tool_name)-arm.>(_target_name).source_list.gypcmd ^(_sources) ^(sources) ^(native_sources))',
                },
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_newlib_arm)',
                'inputs': [
                   '<@(common_inputs)',
                   '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources) >(native_sources))',
                   '>@(extra_deps)',
                   '>@(extra_deps_newlib_arm)',
                   '^(source_list_newlib_arm)',
                   '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json',
                ],
                'outputs': ['>(out_newlib_arm)'],
                'action': [
                  '<@(common_args)',
                  '>@(extra_args)',
                  '--arch', 'arm',
                  '--build', 'newlib_nexe_clang',
                  '--name', '>(out_newlib_arm)',
                  '--objdir', '>(objdir_newlib_arm)',
                  '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                  '--compile_flags=^(compile_flags) >(_compile_flags)',
                  '--gomadir', '<(gomadir)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-B>(tc_lib_dir_newlib_arm) -L>(tc_lib_dir_newlib_arm) ^(link_flags) >(_link_flags)',
                  '--source-list=^(source_list_newlib_arm)',
                ],
              },
            ],
          }],
          # arm newlib library action
          ['nlib_target!="" and build_newlib!=0 and disable_newlib==0', {
            'variables': {
              'tool_name': 'newlib',
              'out_newlib_arm%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/libarm/>(nlib_target)',
              'objdir_newlib_arm%': '>(INTERMEDIATE_DIR)/<(tool_name)-arm/>(_target_name)',
            },
            'actions': [
              {
                'action_name': 'build newlib arm nlib',
                 'variables': {
                   'source_list_newlib_arm%': '^|(<(tool_name)-arm.>(_target_name).source_list.gypcmd ^(_sources) ^(sources) ^(native_sources))',
                 },
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_newlib_arm)',
                'inputs': [
                   '<@(common_inputs)',
                   '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources) >(native_sources))',
                   '>@(extra_deps)',
                   '>@(extra_deps_newlib_arm)',
                   '^(source_list_newlib_arm)',
                   '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json',
                ],
                'outputs': ['>(out_newlib_arm)'],
                'action': [
                  '<@(common_args)',
                  '>@(extra_args)',
                  '--arch', 'arm',
                  '--build', 'newlib_nlib_clang',
                  '--name', '>(out_newlib_arm)',
                  '--objdir', '>(objdir_newlib_arm)',
                  '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                  '--compile_flags=^(compile_flags) >(_compile_flags)',
                  '--gomadir', '<(gomadir)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-B>(tc_lib_dir_newlib_arm) ^(link_flags) >(_link_flags)',
                  '--source-list=^(source_list_newlib_arm)',
                ],
              },
            ],
          }],
          # arm irt nexe action
          ['nexe_target!="" and build_irt!=0', {
            'variables': {
               'tool_name': 'irt',
               'out_newlib_arm%': '<(PRODUCT_DIR)/>(nexe_target)_newlib_arm.nexe',
               'objdir_newlib_arm%': '>(INTERMEDIATE_DIR)/<(tool_name)-arm/>(_target_name)',
            },
            'actions': [
              {
                'action_name': 'build IRT arm nexe',
                'variables': {
                  'source_list_newlib_arm%': '^|(<(tool_name)-arm.>(_target_name).source_list.gypcmd ^(_sources) ^(sources) ^(native_sources))',
                },
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_newlib_arm)',
                'inputs': [
                   '<@(common_inputs)',
                   '<(DEPTH)/native_client/build/link_irt.py',
                   '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources) >(native_sources))',
                   '>@(extra_deps)',
                   '>@(extra_deps_newlib_arm)',
                   '^(source_list_newlib_arm)',
                   '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json',
                   '<(PRODUCT_DIR)/tls_edit<(EXECUTABLE_SUFFIX)',
                ],
                'outputs': ['>(out_newlib_arm)'],
                'action': [
                  '<@(common_args)',
                  '>@(extra_args)',
                  '--arch', 'arm',
                  '--build', 'newlib_nexe_clang',
                  '--name', '>(out_newlib_arm)',
                  '--objdir', '>(objdir_newlib_arm)',
                  '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                  '--compile_flags=^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags) -Wno-unused-local-typedefs ^(native_irt_compile_flags)',
                  '--gomadir', '<(gomadir)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-B>(tc_lib_dir_irt_arm) -L>(tc_lib_dir_irt_arm) ^(native_irt_link_flags) ^(link_flags) >(_link_flags) >(libcpp_irt_stdlibs)',
                  '--source-list=^(source_list_newlib_arm)',
                  '--tls-edit=<(PRODUCT_DIR)/tls_edit<(EXECUTABLE_SUFFIX)',
                  '--irt-linker=<(DEPTH)/native_client/build/link_irt.py',
                ],
              },
            ],
          }],
          # arm IRT library action
          ['nlib_target!="" and build_irt!=0', {
            'variables': {
              'tool_name': 'irt',
              'out_newlib_arm%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/libarm/>(nlib_target)',
              'objdir_newlib_arm%': '>(INTERMEDIATE_DIR)/<(tool_name)-arm/>(_target_name)',
            },
            'actions': [
              {
                'action_name': 'build IRT arm nlib',
                'variables': {
                  'source_list_newlib_arm%': '^|(<(tool_name)-arm.>(_target_name).source_list.gypcmd ^(_sources) ^(sources) ^(native_sources))',
                },
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_newlib_arm)',
                'inputs': [
                   '<@(common_inputs)',
                   '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources) >(native_sources))',
                   '>@(extra_deps)',
                   '>@(extra_deps_newlib_arm)',
                   '^(source_list_newlib_arm)',
                   '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json',
                ],
                'outputs': ['>(out_newlib_arm)'],
                'action': [
                  '<@(common_args)',
                  '>@(extra_args)',
                  '--arch', 'arm',
                  '--build', 'newlib_nlib_clang',
                  '--name', '>(out_newlib_arm)',
                  '--objdir', '>(objdir_newlib_arm)',
                  '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                  '--compile_flags=^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags) -Wno-unused-local-typedefs ^(native_irt_compile_flags)',
                  '--gomadir', '<(gomadir)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-B>(tc_lib_dir_irt_arm) -L>(tc_lib_dir_irt_arm) ^(native_irt_link_flags) ^(link_flags) >(_link_flags) >(libcpp_irt_stdlibs)',
                  '--source-list=^(source_list_newlib_arm)',
                ],
              },
            ],
          }],
          # ARM non-SFI helper nexe build.
          ['nexe_target!="" and build_nonsfi_helper!=0', {
            'variables': {
              'tool_name': 'nonsfi_helper',
              'out_newlib_arm_nonsfi%': '<(PRODUCT_DIR)/>(nexe_target)_newlib_arm_nonsfi.nexe',
              'objdir_newlib_arm_nonsfi%': '>(INTERMEDIATE_DIR)/<(tool_name)-arm-nonsfi/>(_target_name)',
            },
            'actions': [
              {
                'action_name': 'build non-SFI helper ARM nexe',
                'variables': {
                  'source_list_newlib_arm_nonsfi%': '^|(<(tool_name)-arm-nonsfi.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                  'stdlibs': ['-lc++', '-lm', '-lnacl', '-lc', '-lpnaclmm'],
                },
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_newlib_arm_nonsfi)',
                'inputs': [
                  '<@(common_inputs)',
                  '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                  '>@(extra_deps)',
                  '>@(extra_deps_newlib_arm_nonsfi)',
                  '^(source_list_newlib_arm_nonsfi)',
                  '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libnacl_sys_private.a',
                ],
                'outputs': ['>(out_newlib_arm_nonsfi)'],
                'action': [
                  '<@(common_args)',
                  '>@(extra_args)',
                  '--arch', 'arm-nonsfi',
                  '--build', 'newlib_nexe_pnacl',
                  '--name', '>(out_newlib_arm_nonsfi)',
                  '--objdir', '>(objdir_newlib_arm_nonsfi)',
                  '--include-dirs=>(tc_include_dir_newlib) <(DEPTH)/native_client/src/public/linux_syscalls ^(include_dirs) >(_include_dirs)',
                  '--compile_flags=--target=armv7-unknown-nacl-gnueabihf --pnacl-bias=arm-nonsfi ^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags)',
                  '--gomadir', '<(gomadir)',
                  '--defines=^(defines) >(_defines)',
                  # Both -Wl,--noirt and -Wt,--noirt are needed here. The
                  # former is to prevent linking to irt. The latter controls
                  # the entry point for Non-SFI NaCl in pnacl-translate.
                  # "-nodefaultlibs -Wl,--start-group, ... -Wl,--end-group"
                  # is the flags to exclude -lpthread, otherwise it causes
                  # library not found error. Note that pthread related code
                  # is contained in libnacl_sys_private.a, which is
                  # automatically linked.
                  # Place >@(stdlibs) early and group it with libraries from
                  # link_flags. The mixture of native libs and bitcode started
                  # having problems in LLVM 3.6, when stdlibs were grouped
                  # separately and placed later. See:
                  # https://code.google.com/p/nativeclient/issues/detail?id=4067
                  # TODO(jvoung): get to the bottom of this.
                  '--link_flags=--target=armv7-unknown-nacl-gnueabihf -arch arm-nonsfi --pnacl-allow-translate --pnacl-allow-native -Wl,--noirt -Wt,--noirt -Wt,--noirtshim -nodefaultlibs -B>(tc_lib_dir_nonsfi_helper_arm) -Wl,--start-group >@(stdlibs) ^(link_flags) >(_link_flags) -Wl,--end-group',
                  '--source-list=^(source_list_newlib_arm_nonsfi)',
                ],
              },
            ],
          }],
          # ARM non-SFI helper library and PNaCl PPAPI shim for nonsfi build.
          ['nlib_target!="" and (build_nonsfi_helper!=0 or (pnacl_native_biased!=0 and enable_arm_nonsfi==1))', {
            'variables': {
              'tool_name': 'nonsfi_helper',
              'out_newlib_arm_nonsfi%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/libarm/>(nlib_target)',
              'objdir_newlib_arm_nonsfi%': '>(INTERMEDIATE_DIR)/<(tool_name)-arm-nonsfi/>(_target_name)',
            },
            'actions': [
              {
                'action_name': 'build nonsfi_helper arm nlib',
                'variables': {
                  'source_list_newlib_arm_nonsfi%': '^|(<(tool_name)-arm-nonsfi.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                },
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_newlib_arm_nonsfi)',
                'inputs': [
                  '<@(common_inputs)',
                  '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                  '>@(extra_deps)',
                  '>@(extra_deps_newlib_arm_nonsfi)',
                  '^(source_list_newlib_arm_nonsfi)',
                  '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json',
                ],
                'outputs': ['>(out_newlib_arm_nonsfi)'],
                'action': [
                  '<@(common_args)',
                  '>@(extra_args)',
                  '--arch', 'arm-nonsfi',
                  '--build', 'newlib_nlib_pnacl',
                  '--name', '>(out_newlib_arm_nonsfi)',
                  '--objdir', '>(objdir_newlib_arm_nonsfi)',
                  '--include-dirs=>(tc_include_dir_newlib) <(DEPTH)/native_client/src/public/linux_syscalls ^(include_dirs) >(_include_dirs)',
                  '--compile_flags=--target=armv7-unknown-nacl-gnueabihf --pnacl-bias=arm-nonsfi --pnacl-allow-translate --pnacl-allow-native -arch arm-nonsfi -mfloat-abi=hard ^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags)',
                  '--gomadir', '<(gomadir)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-B>(tc_lib_dir_nonsfi_helper_arm) ^(link_flags) >(_link_flags)',
                  '--source-list=^(source_list_newlib_arm_nonsfi)',
                ],
              },
            ],
          }],
        ], # end target_conditions for arm newlib (nexe/nlib)
      },
    }], # end target_arch = arm
    ['target_arch=="mipsel"', {
      'target_defaults': {
        'target_conditions': [
          # mips newlib nexe action
          ['nexe_target!="" and build_newlib!=0 and disable_newlib==0', {
            'variables': {
               'tool_name': 'newlib',
               'out_newlib_mips%': '<(PRODUCT_DIR)/>(nexe_target)_newlib_mips32.nexe',
               'objdir_newlib_mips%': '>(INTERMEDIATE_DIR)/<(tool_name)-mips/>(_target_name)',
            },
            'actions': [
              {
                'action_name': 'build newlib mips nexe',
                'variables': {
                  'source_list_newlib_mips%': '^|(<(tool_name)-mips.>(_target_name).source_list.gypcmd ^(_sources) ^(sources) ^(native_sources))',
                },
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_newlib_mips)',
                'inputs': [
                   '<@(common_inputs)',
                   '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources) >(native_sources))',
                   '>@(extra_deps)',
                   '>@(extra_deps_newlib_mips)',
                   '^(source_list_newlib_mips)',
                   '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json',
                ],
                'outputs': ['>(out_newlib_mips)'],
                'action': [
                  '<@(common_args)',
                  '>@(extra_args)',
                  '--arch', 'mips',
                  '--build', 'newlib_nexe_clang',
                  '--name', '>(out_newlib_mips)',
                  '--objdir', '>(objdir_newlib_mips)',
                  '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                  '--compile_flags=^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags)',
                  '--gomadir', '<(gomadir)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-B>(tc_lib_dir_newlib_mips) -L>(tc_lib_dir_newlib_mips) ^(link_flags) >(_link_flags)',
                  '--source-list=^(source_list_newlib_mips)',
                ],
              },
            ],
          }],
          # mips newlib library action
          ['nlib_target!="" and build_newlib!=0 and disable_newlib==0', {
            'variables': {
              'tool_name': 'newlib',
              'out_newlib_mips%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/libmips/>(nlib_target)',
              'objdir_newlib_mips%': '>(INTERMEDIATE_DIR)/<(tool_name)-mips/>(_target_name)',
            },
            'actions': [
              {
                'action_name': 'build newlib mips nlib',
                'variables': {
                  'source_list_newlib_mips%': '^|(<(tool_name)-mips.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                },
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_newlib_mips)',
                'inputs': [
                   '<@(common_inputs)',
                   '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                   '>@(extra_deps)',
                   '>@(extra_deps_newlib_mips)',
                   '^(source_list_newlib_mips)',
                   '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json',
                ],
                'outputs': ['>(out_newlib_mips)'],
                'action': [
                  '<@(common_args)',
                  '>@(extra_args)',
                  '--arch', 'mips',
                  '--build', 'newlib_nlib_clang',
                  '--name', '>(out_newlib_mips)',
                  '--objdir', '>(objdir_newlib_mips)',
                  '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                  '--compile_flags=^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags)',
                  '--gomadir', '<(gomadir)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-B>(tc_lib_dir_newlib_mips) ^(link_flags) >(_link_flags)',
                  '--source-list=^(source_list_newlib_mips)',
                ],
              },
            ],
          }],
          # mips irt nexe action
          ['nexe_target!="" and build_irt!=0', {
            'variables': {
               'tool_name': 'irt',
               'out_newlib_mips%': '<(PRODUCT_DIR)/>(nexe_target)_newlib_mips32.nexe',
               'objdir_newlib_mips%': '>(INTERMEDIATE_DIR)/<(tool_name)-mips/>(_target_name)',
            },
            'actions': [
              {
                'action_name': 'build IRT mips nexe',
                'variables': {
                  'source_list_newlib_mips%': '^|(<(tool_name)-mips.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                },
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_newlib_mips)',
                'inputs': [
                   '<@(common_inputs)',
                   '<(DEPTH)/native_client/build/link_irt.py',
                   '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                   '>@(extra_deps)',
                   '>@(extra_deps_newlib_mips)',
                   '^(source_list_newlib_mips)',
                   '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json',
                   '<(PRODUCT_DIR)/tls_edit<(EXECUTABLE_SUFFIX)',
                ],
                'outputs': ['>(out_newlib_mips)'],
                'action': [
                  '<@(common_args)',
                  '>@(extra_args)',
                  '--arch', 'mips',
                  '--build', 'newlib_nexe_clang',
                  '--name', '>(out_newlib_mips)',
                  '--objdir', '>(objdir_newlib_mips)',
                  '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                  '--compile_flags=^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags) ^(native_irt_compile_flags)',
                  '--gomadir', '<(gomadir)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-B>(tc_lib_dir_irt_mips) -L>(tc_lib_dir_irt_mips) ^(link_flags) >(_link_flags) ^(native_irt_link_flags) >(libcpp_irt_stdlibs)',
                  '--source-list=^(source_list_newlib_mips)',
                  '--tls-edit=<(PRODUCT_DIR)/tls_edit<(EXECUTABLE_SUFFIX)',
                  '--irt-linker=<(DEPTH)/native_client/build/link_irt.py',
                ],
              },
            ],
          }],
          # mips IRT library action
          ['nlib_target!="" and build_irt!=0', {
            'variables': {
              'tool_name': 'irt',
              'out_newlib_mips%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/libmips/>(nlib_target)',
              'objdir_newlib_mips%': '>(INTERMEDIATE_DIR)/<(tool_name)-mips/>(_target_name)',
            },
            'actions': [
              {
                'action_name': 'build IRT mips nlib',
                'variables': {
                  'source_list_newlib_mips%': '^|(<(tool_name)-mips.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                },
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_newlib_mips)',
                'inputs': [
                   '<@(common_inputs)',
                   '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                   '>@(extra_deps)',
                   '>@(extra_deps_newlib_mips)',
                   '^(source_list_newlib_mips)',
                   '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json',
                ],
                'outputs': ['>(out_newlib_mips)'],
                'action': [
                  '<@(common_args)',
                  '>@(extra_args)',
                  '--arch', 'mips',
                  '--build', 'newlib_nlib_clang',
                  '--name', '>(out_newlib_mips)',
                  '--objdir', '>(objdir_newlib_mips)',
                  '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                  '--compile_flags=^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags) ^(native_irt_compile_flags)',
                  '--gomadir', '<(gomadir)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-B>(tc_lib_dir_irt_mips) -L>(tc_lib_dir_irt_mips) ^(native_irt_link_flags) ^(libcpp_irt_stdlibs) ^(link_flags) >(_link_flags)',
                  '--source-list=^(source_list_newlib_mips)',
                ],
              },
            ],
          }],
        ], # end target_conditions for mips newlib
      },
    }], # end target_arch = mips
    ['target_arch=="ia32" or target_arch=="x64"', {
      'target_defaults': {
        'target_conditions': [
           # x86-64 glibc nexe action
           ['nexe_target!="" and build_glibc!=0 and enable_x86_64!=0 and disable_glibc==0', {
             'variables': {
                'tool_name': 'glibc',
                'out_glibc64%': '<(PRODUCT_DIR)/>(nexe_target)_glibc_x64.nexe',
                'objdir_glibc64%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-64/>(_target_name)',
             },
             'actions': [
               {
                 'action_name': 'build glibc x86-64 nexe',
                 'variables': {
                   'source_list_glibc64%': '^|(<(tool_name)-x86-64.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_glibc64)',
                 'inputs': [
                    '<@(common_inputs)',
                    '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                    '>@(extra_deps)',
                    '>@(extra_deps_glibc64)',
                    '^(source_list_glibc64)',
                    '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/nacl_x86_glibc/nacl_x86_glibc.json',
                 ],
                 'outputs': ['>(out_glibc64)'],
                 'action': [
                   '<@(common_args)',
                   '>@(extra_args)',
                   '--arch', 'x86-64',
                   '--build', 'glibc_nexe',
                   '--name', '>(out_glibc64)',
                   '--objdir', '>(objdir_glibc64)',
                   '--include-dirs=>(tc_include_dir_glibc) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=-m64 ^(compile_flags) >(_compile_flags)',
                   '--gomadir', '<(gomadir)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-B>(tc_lib_dir_glibc64) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_glibc64)',
                 ],
               },
             ],
           }],
           # x86-32 glibc nexe action
           ['nexe_target!="" and build_glibc!=0 and enable_x86_32!=0 and disable_glibc==0', {
             'variables': {
                'tool_name': 'glibc',
                'out_glibc32%': '<(PRODUCT_DIR)/>(nexe_target)_glibc_x32.nexe',
                'objdir_glibc32%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-32/>(_target_name)',
             },
             'actions': [
               {
                 'action_name': 'build glibc x86-32 nexe',
                 'variables': {
                   'source_list_glibc32%': '^|(<(tool_name)-x86-32.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_glibc32)',
                 'inputs': [
                    '<@(common_inputs)',
                    '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                    '>@(extra_deps)',
                    '>@(extra_deps_glibc32)',
                    '^(source_list_glibc32)',
                    '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/nacl_x86_glibc/nacl_x86_glibc.json',
                 ],
                 'outputs': ['>(out_glibc32)'],
                 'action': [
                   '<@(common_args)',
                   '>@(extra_args)',
                   '--arch', 'x86-32',
                   '--build', 'glibc_nexe',
                   '--name', '>(out_glibc32)',
                   '--objdir', '>(objdir_glibc32)',
                   '--include-dirs=>(tc_include_dir_glibc) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=-m32 ^(compile_flags) >(_compile_flags)',
                   '--gomadir', '<(gomadir)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-m32 -B>(tc_lib_dir_glibc32) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_glibc32)',
                 ],
               },
             ],
           }],
           # x86-64 glibc static library action
           ['nlib_target!="" and build_glibc!=0 and enable_x86_64!=0 and disable_glibc==0', {
             'variables': {
                'tool_name': 'glibc',
                'objdir_glibc64%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-64/>(_target_name)',
                'out_glibc64%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib64/>(nlib_target)',
             },
             'actions': [
               {
                 'action_name': 'build glibc x86-64 nlib',
                 'variables': {
                   'source_list_glibc64%': '^|(<(tool_name)-x86-64.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_glibc64)',
                 'inputs': [
                    '<@(common_inputs)',
                    '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                    '>@(extra_deps)',
                    '>@(extra_deps_glibc64)',
                    '^(source_list_glibc64)',
                    '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/nacl_x86_glibc/nacl_x86_glibc.json',
                 ],
                 'outputs': ['>(out_glibc64)'],
                 'action': [
                   '<@(common_args)',
                   '>@(extra_args)',
                   '--arch', 'x86-64',
                   '--build', 'glibc_nlib',
                   '--name', '>(out_glibc64)',
                   '--objdir', '>(objdir_glibc64)',
                   '--include-dirs=>(tc_include_dir_glibc) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=-m64 ^(compile_flags) >(_compile_flags)',
                   '--gomadir', '<(gomadir)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-B>(tc_lib_dir_glibc64) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_glibc64)',
                 ],
               },
             ],
           }],
           # x86-32 glibc static library action
           ['nlib_target!="" and build_glibc!=0 and enable_x86_32!=0 and disable_glibc==0', {
             'variables': {
                'tool_name': 'glibc',
                'out_glibc32%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib32/>(nlib_target)',
                'objdir_glibc32%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-32/>(_target_name)',
             },
             'actions': [
               {
                 'action_name': 'build glibc x86-32 nlib',
                 'variables': {
                   'source_list_glibc32%': '^|(<(tool_name)-x86-32.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_glibc32)',
                 'inputs': [
                    '<@(common_inputs)',
                    '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                    '>@(extra_deps)',
                    '>@(extra_deps_glibc32)',
                    '^(source_list_glibc32)',
                    '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/nacl_x86_glibc/nacl_x86_glibc.json',
                 ],
                 'outputs': ['>(out_glibc32)'],
                 'action': [
                   '<@(common_args)',
                   '>@(extra_args)',
                   '--arch', 'x86-32',
                   '--build', 'glibc_nlib',
                   '--name', '>(out_glibc32)',
                   '--objdir', '>(objdir_glibc32)',
                   '--include-dirs=>(tc_include_dir_glibc) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=-m32 ^(compile_flags) >(_compile_flags)',
                   '--gomadir', '<(gomadir)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-m32 -B>(tc_lib_dir_glibc32) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_glibc32)',
                 ],
               },
             ],
           }],
           # x86-64 glibc shared library action
           ['nso_target!="" and build_glibc!=0 and enable_x86_64!=0 and disable_glibc==0', {
             'variables': {
                'tool_name': 'glibc',
                'objdir_glibc64%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-64-so/>(_target_name)',
                'out_glibc64%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib64/>(nso_target)',
             },
             'actions': [
               {
                 'action_name': 'build glibc x86-64 nso',
                 'variables': {
                   'source_list_glibc64%': '^|(<(tool_name)-x86-64-so.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_glibc64)',
                 'inputs': [
                    '<@(common_inputs)',
                    '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                    '>@(extra_deps)',
                    '>@(extra_deps_glibc64)',
                    '^(source_list_glibc64)',
                    '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/nacl_x86_glibc/nacl_x86_glibc.json',
                 ],
                 'outputs': ['>(out_glibc64)'],
                 'action': [
                   '<@(common_args)',
                   '>@(extra_args)',
                   '--arch', 'x86-64',
                   '--build', 'glibc_nso',
                   '--name', '>(out_glibc64)',
                   '--objdir', '>(objdir_glibc64)',
                   '--include-dirs=>(tc_include_dir_glibc) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=-m64 -fPIC ^(compile_flags) >(_compile_flags)',
                   '--gomadir', '<(gomadir)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-B>(tc_lib_dir_glibc64) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_glibc64)',
                 ],
               },
             ],
           }],
           # x86-32 glibc shared library action
           ['nso_target!="" and build_glibc!=0 and enable_x86_32!=0 and disable_glibc==0', {
             'variables': {
                'tool_name': 'glibc',
                'out_glibc32%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib32/>(nso_target)',
                'objdir_glibc32%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-32-so/>(_target_name)',
             },
             'actions': [
               {
                 'action_name': 'build glibc x86-32 nso',
                 'variables': {
                   'source_list_glibc32%': '^|(<(tool_name)-x86-32-so.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_glibc32)',
                 'inputs': [
                    '<@(common_inputs)',
                    '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                    '>@(extra_deps)',
                    '>@(extra_deps_glibc32)',
                    '^(source_list_glibc32)',
                    '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/nacl_x86_glibc/nacl_x86_glibc.json',
                 ],
                 'outputs': ['>(out_glibc32)'],
                 'action': [
                   '<@(common_args)',
                   '>@(extra_args)',
                   '--arch', 'x86-32',
                   '--build', 'glibc_nso',
                   '--name', '>(out_glibc32)',
                   '--objdir', '>(objdir_glibc32)',
                   '--include-dirs=>(tc_include_dir_glibc) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=-m32 -fPIC ^(compile_flags) >(_compile_flags)',
                   '--gomadir', '<(gomadir)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-m32 -B>(tc_lib_dir_glibc32) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_glibc32)',
                 ],
               },
             ],
           }],
        ], # end target_conditions for glibc (nexe/nlib/nso, x86-32/64)
      },
    }], # end target_arch == ia32 or x64
  ],
  'target_defaults': {
    'pnacl_compile_flags': [],
    'variables': {
      'disable_pnacl%': 0,
      'build_pnacl_newlib': 0,
      # Flag to translate pexes into nexes at build time (vs in the browser).
      'translate_pexe_with_build': 0,
      # Flag to indicate that PNaCl should compile with the native ABI
      # instead of the le32 ABI.
      'pnacl_native_biased': 0,
      'nexe_target': '',
      'nlib_target': '',
      'extra_deps_pnacl_newlib': [],
      'tc_lib_dir_pnacl_newlib': '<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_newlib/lib',
      'tc_lib_dir_pnacl_translate' :'<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_translate',
      'tc_include_dir_pnacl_newlib': '<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_newlib/include',
      'compile_flags': [
        '<@(nacl_default_compile_flags)',
      ],
      'pnacl_compile_flags': [
        '-Wno-extra-semi',
        '-Wno-unused-private-field',
        '-Wno-char-subscripts',
        '-Wno-unused-function',
        '-std=gnu++11',
      ],
      'native_irt_compile_flags': [
        # IRT compile/link flags to make the binary smaller.
        '-Os',
        '-fno-exceptions',
        # These are omitted from non-IRT libraries to keep the libraries
        # themselves small.
        '-ffunction-sections',
        '-fdata-sections',
        # A debugger should be able to unwind IRT call frames. As the IRT is
        # compiled with high level of optimizations and without debug info,
        # compiler is requested to generate unwind tables explicitly. This
        # is the default behavior on x86-64 and when compiling C++ with
        # exceptions enabled, the change is for the benefit of x86-32 C.
        # The frame pointer is unnecessary when unwind tables are used.
        '-fasynchronous-unwind-tables',
        '-fomit-frame-pointer',
      ],
      'native_irt_link_flags': [
        '-Wl,--gc-sections',
      ],
      'pnacl_irt_link_flags': [
        '-Wt,-ffunction-sections',
        '-Wt,-fdata-sections',
        '-Wn,--gc-sections',
      ],
      # The clang driver automatically injects -lpthread when using libc++, but
      # the toolchain doesn't have it yet.  To get around this, use
      # -nodefaultlibs and explicitly link irt_browser_lib to satisfy libc++'s
      # pthread dependency.
      'libcpp_irt_stdlibs': [
        '-nodefaultlibs',
        '-lc++',
        '-lm',
        '-lirt_browser',
        '-Wl,--start-group',
        '-lc',
        '-lnacl',
        '-lgcc',
        '-lgcc_eh',
        '-Wl,--end-group'
      ],
    },
    'target_conditions': [
      # pnacl actions for building pexes and translating them
      ['nexe_target!="" and disable_pnacl==0 and build_pnacl_newlib!=0 '
       'and pnacl_native_biased==0', {
         'variables': {
           'out_pnacl_newlib_x86_32_nexe%': '<(PRODUCT_DIR)/>(nexe_target)_pnacl_newlib_x32.nexe',
           'out_pnacl_newlib_x86_64_nexe%': '<(PRODUCT_DIR)/>(nexe_target)_pnacl_newlib_x64.nexe',
           'out_pnacl_newlib_arm_nexe%': '<(PRODUCT_DIR)/>(nexe_target)_pnacl_newlib_arm.nexe',
           'out_pnacl_newlib_mips_nexe%': '<(PRODUCT_DIR)/>(nexe_target)_pnacl_newlib_mips32.nexe',
           'out_pnacl_newlib_x86_32_nonsfi_nexe%': '<(PRODUCT_DIR)/>(nexe_target)_pnacl_newlib_x32_nonsfi.nexe',
           'out_pnacl_newlib_arm_nonsfi_nexe%': '<(PRODUCT_DIR)/>(nexe_target)_pnacl_newlib_arm_nonsfi.nexe',
           'tool_name': 'pnacl_newlib',
           'inst_dir': '<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_newlib',
           'out_pnacl_newlib%': '<(PRODUCT_DIR)/>(nexe_target)_newlib.pexe',
           'objdir_pnacl_newlib%': '>(INTERMEDIATE_DIR)/<(tool_name)/>(_target_name)',
           'link_flags': [
             '-O3',
           ],
           'translate_flags': [],
         },
         'actions': [
           {
             'action_name': 'build newlib pexe',
             'variables': {
               'source_list_pnacl_newlib%': '^|(<(tool_name).>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
             },
             'msvs_cygwin_shell': 0,
             'description': 'building >(out_pnacl_newlib)',
             'inputs': [
               '<@(common_inputs)',
               '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
               '>@(extra_deps)',
               '>@(extra_deps_pnacl_newlib)',
               '^(source_list_pnacl_newlib)',
               '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json',
             ],
             'outputs': [
               '>(out_pnacl_newlib)',
               '>(out_pnacl_newlib).debug',
             ],
             'action': [
               '<@(common_args)',
               '>@(extra_args)',
               '--arch', 'pnacl',
               '--build', 'newlib_pexe',
               '--name', '>(out_pnacl_newlib)',
               '--objdir', '>(objdir_pnacl_newlib)',
               '--include-dirs=>(tc_include_dir_pnacl_newlib) ^(include_dirs) >(_include_dirs)',
               '--compile_flags=^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags)',
               '--gomadir', '<(gomadir)',
               '--defines=^(defines) >(_defines)',
               '--link_flags=-B<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_newlib/lib ^(link_flags) >(_link_flags)',
               '--source-list=^(source_list_pnacl_newlib)',
             ],
           }],
         'target_conditions': [
           [ 'enable_x86_32!=0 and translate_pexe_with_build!=0', {
             'actions': [{
               'action_name': 'translate newlib pexe to x86-32 nexe',
               'msvs_cygwin_shell': 0,
               'description': 'translating >(out_pnacl_newlib_x86_32_nexe)',
               'inputs': [
                 # Depending on out_pnacl_newlib helps depend on common_inputs.
                 '>(out_pnacl_newlib)',
               ],
               'outputs': [ '>(out_pnacl_newlib_x86_32_nexe)' ],
               'action' : [
                 '<@(common_args)',
                 '--arch', 'x86-32',
                 '--build', 'newlib_translate',
                 '--name', '>(out_pnacl_newlib_x86_32_nexe)',
                 '--link_flags=^(translate_flags) >(translate_flags) -Wl,-L>(tc_lib_dir_pnacl_translate)/lib-x86-32',
                 '>(out_pnacl_newlib)',
               ],
             }],
           }],
           [ 'enable_x86_32_nonsfi!=0 and translate_pexe_with_build!=0', {
             'actions': [{
               'action_name': 'translate newlib pexe to x86-32-nonsfi nexe',
               'msvs_cygwin_shell': 0,
               'description': 'translating >(out_pnacl_newlib_x86_32_nonsfi_nexe)',
               'inputs': [
                 # Depending on out_pnacl_newlib helps depend on common_inputs.
                 '>(out_pnacl_newlib)',
               ],
               'outputs': [ '>(out_pnacl_newlib_x86_32_nonsfi_nexe)' ],
               'action' : [
                 '<@(common_args)',
                 '--arch', 'x86-32-nonsfi',
                 '--build', 'newlib_translate',
                 '--name', '>(out_pnacl_newlib_x86_32_nonsfi_nexe)',
                 '--link_flags=^(translate_flags) >(translate_flags) -Wl,-L>(tc_lib_dir_pnacl_translate)/lib-x86-32-nonsfi',
                 '>(out_pnacl_newlib)',
               ],
             }],
           }],
           [ 'enable_x86_64!=0 and translate_pexe_with_build!=0', {
             'actions': [{
               'action_name': 'translate newlib pexe to x86-64 nexe',
               'msvs_cygwin_shell': 0,
               'description': 'translating >(out_pnacl_newlib_x86_64_nexe)',
               'inputs': [
                 # Depending on out_pnacl_newlib helps depend on common_inputs.
                 '>(out_pnacl_newlib)',
               ],
               'outputs': [ '>(out_pnacl_newlib_x86_64_nexe)' ],
               'action' : [
                 '<@(common_args)',
                 '--arch', 'x86-64',
                 '--build', 'newlib_translate',
                 '--name', '>(out_pnacl_newlib_x86_64_nexe)',
                 '--link_flags=^(translate_flags) >(translate_flags) -Wl,-L>(tc_lib_dir_pnacl_translate)/lib-x86-64',
                 '>(out_pnacl_newlib)',
               ],
             }],
           }],
           [ 'enable_arm!=0 and translate_pexe_with_build!=0', {
             'actions': [{
               'action_name': 'translate newlib pexe to ARM nexe',
               'msvs_cygwin_shell': 0,
               'description': 'translating >(out_pnacl_newlib_arm_nexe)',
               'inputs': [
                 # Depending on out_pnacl_newlib helps depend on common_inputs.
                 '>(out_pnacl_newlib)',
               ],
               'outputs': [ '>(out_pnacl_newlib_arm_nexe)' ],
               'action' : [
                 '<@(common_args)',
                 '--arch', 'arm',
                 '--build', 'newlib_translate',
                 '--name', '>(out_pnacl_newlib_arm_nexe)',
                 '--link_flags=^(translate_flags) >(translate_flags) -Wl,-L>(tc_lib_dir_pnacl_translate)/lib-arm',
                 '>(out_pnacl_newlib)',
               ],
             }],
           }],
           [ 'enable_arm_nonsfi!=0 and translate_pexe_with_build!=0', {
             'actions': [{
               'action_name': 'translate newlib pexe to ARM-nonsfi nexe',
               'msvs_cygwin_shell': 0,
               'description': 'translating >(out_pnacl_newlib_arm_nonsfi_nexe)',
               'inputs': [
                 # Depending on out_pnacl_newlib helps depend on common_inputs.
                 '>(out_pnacl_newlib)',
               ],
               'outputs': [ '>(out_pnacl_newlib_arm_nonsfi_nexe)' ],
               'action' : [
                 '<@(common_args)',
                 '--arch', 'arm-nonsfi',
                 '--build', 'newlib_translate',
                 '--name', '>(out_pnacl_newlib_arm_nonsfi_nexe)',
                 '--link_flags=^(translate_flags) >(translate_flags) -Wl,-L>(tc_lib_dir_pnacl_translate)/lib-arm-nonsfi',
                 '>(out_pnacl_newlib)',
               ],
             }],
           }],
           [ 'enable_mips!=0 and translate_pexe_with_build!=0', {
             'actions': [{
               'action_name': 'translate newlib pexe to MIPS nexe',
               'msvs_cygwin_shell': 0,
               'description': 'translating >(out_pnacl_newlib_mips_nexe)',
               'inputs': [
                 # Depending on out_pnacl_newlib helps depend on common_inputs.
                 '>(out_pnacl_newlib)',
               ],
               'outputs': [ '>(out_pnacl_newlib_mips_nexe)' ],
               'action' : [
                 '<@(common_args)',
                 '--arch', 'mips',
                 '--build', 'newlib_translate',
                 '--name', '>(out_pnacl_newlib_mips_nexe)',
                 '--link_flags=^(translate_flags) >(translate_flags) -Wl,-L>(tc_lib_dir_pnacl_translate)/lib-mips32',
                 '>(out_pnacl_newlib)',
               ],
             }],
           }],
         ],
       }],  # end pnacl actions for building pexes and translating to nexes
      # pnacl action for building portable bitcode libraries
      ['nlib_target!="" and disable_pnacl==0 and build_pnacl_newlib!=0'
       'and pnacl_native_biased==0', {
         'variables': {
           'tool_name': 'pnacl_newlib',
           'objdir_pnacl_newlib%': '>(INTERMEDIATE_DIR)/<(tool_name)-pnacl/>(_target_name)',
           'out_pnacl_newlib%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib/>(nlib_target)',
         },
         'actions': [
           {
             'action_name': 'build newlib plib',
             'variables': {
               'source_list_pnacl_newlib%': '^|(<(tool_name).>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
             },
             'msvs_cygwin_shell': 0,
             'description': 'building >(out_pnacl_newlib)',
             'inputs': [
               '<@(common_inputs)',
               '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
               '>@(extra_deps)',
               '>@(extra_deps_pnacl_newlib)',
               '^(source_list_pnacl_newlib)',
               '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json',
             ],
             'outputs': ['>(out_pnacl_newlib)'],
             'action': [
               '<@(common_args)',
               '>@(extra_args)',
               '--arch', 'pnacl',
               '--build', 'newlib_plib',
               '--name', '>(out_pnacl_newlib)',
               '--objdir', '>(objdir_pnacl_newlib)',
               '--include-dirs=>(tc_include_dir_pnacl_newlib) ^(include_dirs) >(_include_dirs)',
               '--compile_flags=^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags)',
               '--gomadir', '<(gomadir)',
               '--defines=^(defines) >(_defines)',
               '--link_flags=-B>(tc_lib_dir_pnacl_newlib) ^(link_flags) >(_link_flags)',
               '--source-list=^(source_list_pnacl_newlib)',
             ],
           },
         ],
       }], # end pnacl actions for bitcode libraries
    ], # end target conditions for pnacl pexe/plib
    # pnacl actions for building ABI-biased native libraries
    'conditions': [
      # ARM
      ['target_arch=="arm"', {
        'target_conditions': [
          ['disable_pnacl==0 and pnacl_native_biased==1 and nlib_target!="" and build_pnacl_newlib!=0', {
            'variables': {
               'tool_name': 'pnacl_newlib_arm',
               'out_pnacl_newlib_arm%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/libarm/>(nlib_target)',
               'objdir_pnacl_newlib_arm%': '>(INTERMEDIATE_DIR)/<(tool_name)/>(_target_name)',
             },
            'actions': [
              {
                'action_name': 'build newlib arm nlib (via pnacl)',
                'variables': {
                  'source_list_pnacl_newlib_arm%': '^|(<(tool_name).>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                },
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_pnacl_newlib_arm)',
                'inputs': [
                  '<@(common_inputs)',
                  '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                  '>@(extra_deps)',
                  '>@(extra_deps_pnacl_newlib)',
                  '^(source_list_pnacl_newlib_arm)',
                  '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json'
                ],
                'outputs': ['>(out_pnacl_newlib_arm)'],
                'action': [
                  '<@(common_args)',
                  '>@(extra_args)',
                  '--arch', 'arm',
                  '--build', 'newlib_nlib_pnacl',
                  '--name', '>(out_pnacl_newlib_arm)',
                  '--objdir', '>(objdir_pnacl_newlib_arm)',
                  '--include-dirs=>(tc_include_dir_pnacl_newlib) ^(include_dirs) >(_include_dirs)',
                  '--compile_flags=--target=armv7-unknown-nacl-gnueabi -mfloat-abi=hard --pnacl-allow-translate -arch arm ^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags)',
                  '--gomadir', '<(gomadir)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-B>(tc_lib_dir_pnacl_newlib) ^(link_flags) >(_link_flags)',
                  '--source-list=^(source_list_pnacl_newlib_arm)',
                ],
              },
            ],
          }],
        ],
      }], # end ARM
      # ia32 or x64 (want to build both for Windows)
      ['target_arch=="ia32" or target_arch=="x64"', {
        'target_conditions': [
          # x64
          ['enable_x86_64!=0 and disable_pnacl==0 and pnacl_native_biased==1 '
           'and nlib_target!="" and build_pnacl_newlib!=0', {
             'variables': {
               'tool_name': 'pnacl_newlib_x86_64',
               'out_pnacl_newlib_x86_64%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib-x86-64/>(nlib_target)',
               'objdir_pnacl_newlib_x86_64%': '>(INTERMEDIATE_DIR)/<(tool_name)/>(_target_name)',
             },
             'actions': [
               {
                 'action_name': 'build newlib x86-64 nlib (via pnacl)',
                 'variables': {
                   'source_list_pnacl_newlib_x86_64%': '^|(<(tool_name).>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_pnacl_newlib_x86_64)',
                 'inputs': [
                   '<@(common_inputs)',
                   '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                   '>@(extra_deps)',
                   '>@(extra_deps_pnacl_newlib)',
                   '^(source_list_pnacl_newlib_x86_64)',
                   '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json'
                 ],
                 'outputs': ['>(out_pnacl_newlib_x86_64)'],
                 'action': [
                   '<@(common_args)',
                   '>@(extra_args)',
                   '--arch', 'x86-64',
                   '--build', 'newlib_nlib_pnacl',
                   '--name', '>(out_pnacl_newlib_x86_64)',
                   '--objdir', '>(objdir_pnacl_newlib_x86_64)',
                   '--include-dirs=>(tc_include_dir_pnacl_newlib) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=--target=x86_64-unknown-nacl --pnacl-allow-translate -arch x86-64 ^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags)',
                   '--gomadir', '<(gomadir)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-B>(tc_lib_dir_pnacl_newlib) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_pnacl_newlib_x86_64)',
                 ],
               },
             ],
           }], # end x64
          # ia32
          ['enable_x86_32!=0 and disable_pnacl==0 and pnacl_native_biased==1 '
           'and nlib_target!="" and build_pnacl_newlib!=0', {
             'variables': {
               'tool_name': 'pnacl_newlib_x86_32',
               'out_pnacl_newlib_x86_32%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib-x86-32/>(nlib_target)',
               'objdir_pnacl_newlib_x86_32%': '>(INTERMEDIATE_DIR)/<(tool_name)/>(_target_name)',
             },
             'actions': [
               {
                 'action_name': 'build newlib x86-32 nlib (via pnacl)',
                 'variables': {
                   'source_list_pnacl_newlib_x86_32%': '^|(<(tool_name).>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_pnacl_newlib_x86_32)',
                 'inputs': [
                   '<@(common_inputs)',
                   '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                   '>@(extra_deps)',
                   '>@(extra_deps_pnacl_newlib)',
                   '^(source_list_pnacl_newlib_x86_32)',
                   '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json'
                 ],
                 'outputs': ['>(out_pnacl_newlib_x86_32)'],
                 'action': [
                   '<@(common_args)',
                   '>@(extra_args)',
                   '--arch', 'x86-32',
                   '--build', 'newlib_nlib_pnacl',
                   '--name', '>(out_pnacl_newlib_x86_32)',
                   '--objdir', '>(objdir_pnacl_newlib_x86_32)',
                   '--include-dirs=>(tc_include_dir_pnacl_newlib) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=--target=i686-unknown-nacl --pnacl-allow-translate -arch x86-32 ^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags)',
                   '--gomadir', '<(gomadir)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-B>(tc_lib_dir_pnacl_newlib) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_pnacl_newlib_x86_32)',
                 ],
               },
             ],
           }], # end ia32
        ], # end ia32 or x64
      }],
      # MIPS
      # The shim is not biased since the IRT itself is not biased.
      ['target_arch=="mipsel"', {
        'target_conditions': [
          ['disable_pnacl==0 and pnacl_native_biased==1 and nlib_target!="" and build_pnacl_newlib!=0', {
            'variables': {
               'tool_name': 'pnacl_newlib_mips',
               'out_pnacl_newlib_mips%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/libmips/>(nlib_target)',
               'objdir_pnacl_newlib_mips%': '>(INTERMEDIATE_DIR)/<(tool_name)/>(_target_name)',
             },
            'actions': [
              {
                'action_name': 'build newlib mips nlib (via pnacl)',
                'variables': {
                  'source_list_pnacl_newlib_mips%': '^|(<(tool_name).>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                },
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_pnacl_newlib_mips)',
                'inputs': [
                  '<@(common_inputs)',
                  '>!@pymod_do_main(scan_sources -I . >(include_dirs) >(_include_dirs) -S >(sources) >(_sources))',
                  '>@(extra_deps)',
                  '>@(extra_deps_pnacl_newlib)',
                  '^(source_list_pnacl_newlib_mips)',
                  '<(DEPTH)/native_client/toolchain/<(TOOLCHAIN_OS)_x86/pnacl_newlib/pnacl_newlib.json'
                ],
                'outputs': ['>(out_pnacl_newlib_mips)'],
                'action': [
                  '<@(common_args)',
                  '>@(extra_args)',
                  '--arch', 'mips',
                  '--build', 'newlib_nlib_clang',
                  '--name', '>(out_pnacl_newlib_mips)',
                  '--objdir', '>(objdir_pnacl_newlib_mips)',
                  '--include-dirs=>(tc_include_dir_pnacl_newlib) ^(include_dirs) >(_include_dirs)',
                  '--compile_flags=^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags)',
                  '--gomadir', '<(gomadir)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-B>(tc_lib_dir_pnacl_newlib) ^(link_flags) >(_link_flags)',
                  '--source-list=^(source_list_pnacl_newlib_mips)',
                ],
              },
            ],
          }],
        ],
      }], # end MIPS
      # end pnacl actions for building ABI-biased native libraries
    ], # end conditions for pnacl biased nlib
  },
}
