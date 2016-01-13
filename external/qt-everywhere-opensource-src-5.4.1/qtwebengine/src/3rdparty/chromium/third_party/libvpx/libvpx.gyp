# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'libvpx_build_vp9%': 1,
    'libvpx_source%': 'source/libvpx',
    'conditions': [
      ['os_posix==1', {
        'asm_obj_extension': 'o',
      }],
      ['OS=="win"', {
        'asm_obj_extension': 'obj',
      }],

      ['msan==1', {
        'target_arch_full': 'generic',
      }, {
        'conditions': [
          ['(target_arch=="arm" or target_arch=="armv7") and arm_neon==1', {
            'target_arch_full': 'arm-neon',
          }, {
            'conditions': [
              ['OS=="android" and ((target_arch=="arm" or target_arch=="armv7") and arm_neon==0)', {
                'target_arch_full': 'arm-neon-cpu-detect',
              }, {
               'target_arch_full': '<(target_arch)',
              }],
            ],
          }],
          ['target_arch=="arm64"', {
            'target_arch_full': 'arm64',
          }],
        ],
      }],

      ['os_posix == 1 and OS != "mac"', {
        'OS_CATEGORY%': 'linux',
      }, {
        'OS_CATEGORY%': '<(OS)',
      }],
    ],

    # Location of the intermediate output.
    'shared_generated_dir': '<(SHARED_INTERMEDIATE_DIR)/third_party/libvpx',
  },
  'target_defaults': {
    'target_conditions': [
      ['<(libvpx_build_vp9)==0', {
        'sources/': [ ['exclude', '(^|/)vp9/'], ],
      }],
    ],
    'variables': {
      'conditions': [
        ['OS=="win" and buildtype=="Official"', {
          # Do not set to 'size', as it results in an error on win64.
          'optimize' :'speed',
        }],
      ],
    },
  },
  'conditions': [
    ['target_arch=="ia32"', {
      'includes': ['libvpx_srcs_x86_intrinsics.gypi', ],
    }],
    ['target_arch=="x64" and msan==0', {
      'includes': ['libvpx_srcs_x86_64_intrinsics.gypi', ],
    }],
    [ '(target_arch=="arm" or target_arch=="armv7") and arm_neon==0 and OS=="android"', {
      # When building for targets which may not have NEON (but may!), include
      # support for neon and hide it behind Android cpu-features.
      'includes': ['libvpx_srcs_arm_neon_cpu_detect_intrinsics.gypi', ],
    }],
    [ '(target_arch != "arm" and target_arch != "armv7") and target_arch != "mipsel"', {
      'targets': [
        {
          # This libvpx target contains both encoder and decoder.
          # Encoder is configured to be realtime only.
          'target_name': 'libvpx',
          'type': 'static_library',
          'variables': {
            'yasm_output_path': '<(SHARED_INTERMEDIATE_DIR)/third_party/libvpx',
            'OS_CATEGORY%': '<(OS_CATEGORY)',
            'yasm_flags': [
              '-D', 'CHROMIUM',
              '-I', 'source/config/<(OS_CATEGORY)/<(target_arch_full)',
              '-I', 'source/config',
              '-I', '<(libvpx_source)',
              '-I', '<(shared_generated_dir)', # Generated assembly offsets
            ],
          },
          'dependencies': [
            'gen_asm_offsets_vp8',
          ],
          'includes': [
            '../yasm/yasm_compile.gypi'
          ],
          'include_dirs': [
            'source/config/<(OS_CATEGORY)/<(target_arch_full)',
            'source/config',
            '<(libvpx_source)',
            '<(libvpx_source)/vp8/common',
            '<(libvpx_source)/vp8/decoder',
            '<(libvpx_source)/vp8/encoder',
            '<(shared_generated_dir)', # Provides vpx_rtcd.h.
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(libvpx_source)',
            ],
          },
          # VS2010 does not correctly incrementally link obj files generated
          # from asm files. This flag disables UseLibraryDependencyInputs to
          # avoid this problem.
          'msvs_2010_disable_uldi_when_referenced': 1,
          'conditions': [
            ['target_arch=="ia32"', {
              'includes': [
                'libvpx_srcs_x86.gypi',
              ],
              'dependencies': [
                'libvpx_intrinsics_mmx',
                'libvpx_intrinsics_sse2',
                # Currently no sse3 intrinsic functions
                #'libvpx_intrinsics_sse3',
                'libvpx_intrinsics_ssse3',
                # Currently no sse4_1 intrinsic functions
                #'libvpx_intrinsics_sse4_1',
                # Currently no avx intrinsic functions
                #'libvpx_intrinsics_avx',
                #'libvpx_intrinsics_avx2',
              ],
            }],
            ['target_arch=="arm64"', {
              'includes': [ 'libvpx_srcs_arm64.gypi', ],
            }],
            ['target_arch=="x64"', {
              'conditions': [
                ['msan==1', {
                  'includes': [ 'libvpx_srcs_generic.gypi', ],
                }, {
                  'includes': [
                    'libvpx_srcs_x86_64.gypi',
                    'libvpx_srcs_nacl.gypi',
                  ],
                  'dependencies': [
                    'libvpx_intrinsics_mmx',
                    'libvpx_intrinsics_sse2',
                    # Currently no sse3 intrinsic functions
                    #'libvpx_intrinsics_sse3',
                    'libvpx_intrinsics_ssse3',
                    # Currently no sse4_1 intrinsic functions
                    #'libvpx_intrinsics_sse4_1',
                    # Currently no avx intrinsic functions
                    #'libvpx_intrinsics_avx',
                    #'libvpx_intrinsics_avx2',
                  ],
                }],
              ],
            }],
            ['clang == 1', {
              'xcode_settings': {
                'WARNING_CFLAGS': [
                  # libvpx heavily relies on implicit enum casting.
                  '-Wno-conversion',
                  # libvpx does `if ((a == b))` in some places.
                  '-Wno-parentheses-equality',
                ],
              },
              'cflags': [
                '-Wno-conversion',
                '-Wno-parentheses-equality',
              ],
            }],
            ['chromeos == 1', {
              # ChromeOS needs these files for animated WebM avatars.
              'sources': [
                '<(libvpx_source)/third_party/libmkv/EbmlIDs.h',
                '<(libvpx_source)/third_party/libmkv/EbmlWriter.c',
                '<(libvpx_source)/third_party/libmkv/EbmlWriter.h',
              ],
            }],
          ],
        },
      ],
    },
    ],
    # 'libvpx' target for mips builds.
    [ 'target_arch=="mipsel" ', {
      'targets': [
        {
          # This libvpx target contains both encoder and decoder.
          # Encoder is configured to be realtime only.
          'target_name': 'libvpx',
          'type': 'static_library',
          'variables': {
            'shared_generated_dir':
              '<(SHARED_INTERMEDIATE_DIR)/third_party/libvpx',
          },
          'includes': [
            'libvpx_srcs_mips.gypi',
          ],
          'include_dirs': [
            'source/config/<(OS_CATEGORY)/<(target_arch_full)',
            'source/config',
            '<(libvpx_source)',
            '<(libvpx_source)/vp8/common',
            '<(libvpx_source)/vp8/decoder',
            '<(libvpx_source)/vp8/encoder',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(libvpx_source)',
            ],
          },
          'sources': [
            'source/config/<(OS_CATEGORY)/<(target_arch_full)/vpx_config.c',
          ],
        },
      ],
    },
    ],
    # 'libvpx' target for ARM builds.
    [ '(target_arch=="arm" or target_arch=="armv7") ', {
      'targets': [
        {
          # This libvpx target contains both encoder and decoder.
          # Encoder is configured to be realtime only.
          'target_name': 'libvpx',
          'type': 'static_library',
          'dependencies': [
            'gen_asm_offsets_vp8',
            'gen_asm_offsets_vpx_scale',
          ],

          # Copy the script to the output folder so that we can use it with
          # absolute path.
          'copies': [{
            'destination': '<(shared_generated_dir)',
            'files': [
              '<(ads2gas_script_path)',
              '<(ads2gas_script_include)',
            ],
          }],

          # Rule to convert .asm files to .S files.
          'rules': [
            {
              'rule_name': 'convert_asm',
              'extension': 'asm',
              'inputs': [
                '<(shared_generated_dir)/<(ads2gas_script)',
                '<(shared_generated_dir)/thumb.pm',
              ],
              'outputs': [
                '<(shared_generated_dir)/<(RULE_INPUT_ROOT).S',
              ],
              'action': [
                'bash',
                '-c',
                'cat <(RULE_INPUT_PATH) | perl <(shared_generated_dir)/<(ads2gas_script) -chromium > <(shared_generated_dir)/<(RULE_INPUT_ROOT).S',
              ],
              'process_outputs_as_sources': 1,
              'message': 'Convert libvpx asm file for ARM <(RULE_INPUT_PATH)',
            },
          ],

          'variables': {
            'variables': {
              'conditions': [
                ['OS=="ios"', {
                  'ads2gas_script%': 'ads2gas_apple.pl',
                }, {
                  'ads2gas_script%': 'ads2gas.pl',
                }],
              ],
            },
            'ads2gas_script%': '<(ads2gas_script)',
            # Location of the assembly conversion script.
            'ads2gas_script_path': '<(libvpx_source)/build/make/<(ads2gas_script)',
            'ads2gas_script_include': '<(libvpx_source)/build/make/thumb.pm',
          },
          'xcode_settings': {
            'OTHER_CFLAGS': [
              '-I<!(pwd)/source/config/<(OS_CATEGORY)/<(target_arch_full)',
              '-I<!(pwd)/source/config',
              '-I<(shared_generated_dir)',
            ],
          },
          'include_dirs': [
            'source/config/<(OS_CATEGORY)/<(target_arch_full)',
            'source/config',
            '<(libvpx_source)',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(libvpx_source)',
            ],
          },
          # We need to explicitly tell the assembler to look for
          # .include directive files from the place where they're
          # generated to.
          'cflags': [
             '-Wa,-I,<(shared_generated_dir)',
          ],
          'conditions': [
            # For Android WebView, the following pathc are not required and not
            # allowed, because they generate the absolute path.
            ['android_webview_build!=1', {
              'cflags': [
                '-Wa,-I,<!(pwd)/source/config/<(OS_CATEGORY)/<(target_arch_full)',
                '-Wa,-I,<!(pwd)/source/config',
              ],
            }],
            # Libvpx optimizations for ARMv6 or ARMv7 without NEON.
            ['arm_neon==0', {
              'conditions': [
                ['OS=="android"', {
                  'includes': [
                    'libvpx_srcs_arm_neon_cpu_detect.gypi',
                  ],
                  'dependencies': [
                    'libvpx_intrinsics_neon',
		  ],
                  'cflags': [
                    '-Wa,-mfpu=neon',
                  ],
                }, {
                  'includes': [
                    'libvpx_srcs_arm.gypi',
                  ],
                }],
              ],
            }],
            # Libvpx optimizations for ARMv7 with NEON.
            ['arm_neon==1', {
              'includes': [
                'libvpx_srcs_arm_neon.gypi',
              ],
            }],
            ['OS == "android"', {
              'includes': [
                '../../build/android/cpufeatures.gypi',
              ],
            }],
            ['chromeos == 1', {
              # ChromeOS needs these files for animated WebM avatars.
              'sources': [
                '<(libvpx_source)/third_party/libmkv/EbmlIDs.h',
                '<(libvpx_source)/third_party/libmkv/EbmlWriter.c',
                '<(libvpx_source)/third_party/libmkv/EbmlWriter.h',
              ],
            }],
            ['OS == "ios"', {
              'xcode_settings': {
                'OTHER_CFLAGS!': [
                  # Breaks at least boolhuff_armv5te:token_high_bit_not_set_ev.
                  '-fstack-protector-all',  # Implies -fstack-protector
                ],
              },
            }],
          ],
        },
      ],
    }],
  ],
  'targets': [
    {
      # A tool that runs on host to extract integers from object file.
      'target_name': 'libvpx_obj_int_extract',
      'type': 'executable',
      'toolsets': ['host'],
      'include_dirs': [
        'source/config/<(OS_CATEGORY)/<(target_arch_full)',
        'source/config',
        '<(libvpx_source)',
      ],
      'sources': [
        '<(libvpx_source)/build/make/obj_int_extract.c',
      ],
      'conditions': [
        ['android_webview_build==1', {
          'defines': [ 'FORCE_PARSE_ELF' ],
          'include_dirs': [ 'include' ],
        }],
      ],
    },
    {
      # A library that contains assembly offsets needed.
      'target_name': 'libvpx_asm_offsets_vp8',
      'type': 'static_library',
      'android_unmangled_name': 1,
      'hard_dependency': 1,
      'include_dirs': [
        'source/config/<(OS_CATEGORY)/<(target_arch_full)',
        'source/config',
        '<(libvpx_source)',
      ],
      'conditions': [
        ['asan==1', {
          'cflags!': [ '-fsanitize=address' ],
          'xcode_settings': { 'OTHER_CFLAGS!': [ '-fsanitize=address' ] },
          'ldflags!': [ '-fsanitize=address' ],
        }],
        ['OS=="win"', {
          'msvs_settings': {
            'VCCLCompilerTool': {
              # Clang uses -fdata-sections when function level linking is
              # enabled, but libvpx_obj_int_extract expects all data to
              # reside in the same section on Windows.
              'EnableFunctionLevelLinking': 'false',
            },
          },
        }],
      ],
      'sources': [
        '<(libvpx_source)/vp8/encoder/vp8_asm_enc_offsets.c',
      ],
    },
    {
      # A library that contains assembly offsets needed.
      # TODO(fgalligan): Merge libvpx_asm_offsets_vpx_scale into
      # libvpx_asm_offsets.
      'target_name': 'libvpx_asm_offsets_vpx_scale',
      'type': 'static_library',
      'android_unmangled_name': 1,
      'hard_dependency': 1,
      'include_dirs': [
        'source/config/<(OS_CATEGORY)/<(target_arch_full)',
        'source/config',
        '<(libvpx_source)',
      ],
      'conditions': [
        ['asan==1', {
          'cflags!': [ '-faddress-sanitizer', '-fsanitize=address', ],
          'xcode_settings': {
            'OTHER_CFLAGS!': [ '-faddress-sanitizer','-fsanitize=address' ],
          },
          'ldflags!': [ '-faddress-sanitizer', '-fsanitize=address', ],
        }],
      ],
      'sources': [
        '<(libvpx_source)/vpx_scale/vpx_scale_asm_offsets.c',
      ],
    },
    {
      # A target that takes assembly offsets library and generate the
      # corresponding assembly files.
      # This target is a hard dependency because the generated .asm files
      # are needed all assembly optimized files in libvpx.
      'target_name': 'gen_asm_offsets_vp8',
      'type': 'none',
      'hard_dependency': 1,
      'dependencies': [
        'libvpx_asm_offsets_vp8',
        'libvpx_obj_int_extract#host',
      ],
      'variables' : {
        'lib_intermediate_name' : '',
        'output_format':'',
        'output_dir': '<(shared_generated_dir)',
        'conditions' : [
          ['android_webview_build==1', {
            # pass the empty string for 3rd and 4th arguments of
            # intermediates-dir-for macro.
            'lib_intermediate_name' : '$(abspath $(call intermediates-dir-for,STATIC_LIBRARIES,libvpx_asm_offsets_vp8,,,$(gyp_var_prefix)))/libvpx_asm_offsets_vp8.a',
          }],
          ['(target_arch=="arm" or target_arch=="armv7")', {
            'output_format': 'gas',
          }, {
            'output_format': 'rvds',
          }],
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'variables': {
            'ninja_obj_dir': '<(PRODUCT_DIR)/obj/third_party/libvpx/<(libvpx_source)/vp8',
          },
          'actions': [
            {
              'action_name': 'copy_enc_offsets_obj',
              'inputs': [ 'copy_obj.py' ],
              'outputs': [ '<(INTERMEDIATE_DIR)/vp8_asm_enc_offsets.obj' ],
              'action': [
                'python',
                '<(DEPTH)/third_party/libvpx/copy_obj.py',
                '-d', '<@(_outputs)',
                '-s', '<(PRODUCT_DIR)/obj/libvpx_asm_offsets_vp8/vp8_asm_enc_offsets.obj',
                '-s', '<(ninja_obj_dir)/encoder/libvpx_asm_offsets_vp8.vp8_asm_enc_offsets.obj',
                '-s', '<(PRODUCT_DIR)/obj/Source/WebKit/chromium/third_party/libvpx/<(libvpx_source)/vp8/encoder/libvpx_asm_offsets_vp8.vp8_asm_enc_offsets.obj',
                '-s', '<(qtwe_chromium_obj_dir)/third_party/libvpx/<(libvpx_source)/vp8/encoder/libvpx_asm_offsets_vp8.vp8_asm_enc_offsets.obj',
              ],
              'process_output_as_sources': 1,
            },
          ],
          'sources': [
            '<(INTERMEDIATE_DIR)/vp8_asm_enc_offsets.obj',
          ],
        }, {
          'variables': {
            'unpack_lib_search_path_list': [
              '-a', '<(PRODUCT_DIR)/libvpx_asm_offsets_vp8.a',
              '-a', '<(LIB_DIR)/third_party/libvpx/libvpx_asm_offsets_vp8.a',
              '-a', '<(LIB_DIR)/Source/WebKit/chromium/third_party/libvpx/libvpx_asm_offsets_vp8.a',
              '-a', '<(lib_intermediate_name)',
            ],
            'unpack_lib_output_dir':'<(INTERMEDIATE_DIR)',
            'unpack_lib_name':'vp8_asm_enc_offsets.o'
          },
          'includes': ['unpack_lib_posix.gypi'],
          # Need this otherwise gyp won't run the rule on them.
          'sources': [
            '<(INTERMEDIATE_DIR)/vp8_asm_enc_offsets.o',
          ],
        }],
      ],
      'includes': ['obj_int_extract.gypi'],
    },
    {
      # A target that takes assembly offsets library and generate the
      # corresponding assembly files.
      # This target is a hard dependency because the generated .asm files
      # are needed all assembly optimized files in libvpx.
      'target_name': 'gen_asm_offsets_vpx_scale',
      'type': 'none',
      'hard_dependency': 1,
      'dependencies': [
        'libvpx_asm_offsets_vpx_scale',
        'libvpx_obj_int_extract#host',
      ],
      'variables' : {
        'lib_intermediate_name' : '',
        'output_format':'',
        'output_dir': '<(shared_generated_dir)',
        'conditions' : [
          ['android_webview_build==1', {
            # pass the empty string for 3rd and 4th arguments of
            # intermediates-dir-for macro.
            'lib_intermediate_name' : '$(abspath $(call intermediates-dir-for,STATIC_LIBRARIES,libvpx_asm_offsets_vpx_scale,,,$(gyp_var_prefix)))/libvpx_asm_offsets_vpx_scale.a',
          }],
          ['(target_arch=="arm" or target_arch=="armv7")', {
            'output_format': 'gas',
          }, {
            'output_format': 'rvds',
          }],
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'variables': {
            'ninja_obj_dir': '<(PRODUCT_DIR)/obj/third_party/libvpx/<(libvpx_source)/vpx_scale',
          },
          'actions': [
            {
              'action_name': 'copy_enc_offsets_obj',
              'inputs': [ 'copy_obj.py' ],
              'outputs': [ '<(INTERMEDIATE_DIR)/vpx_scale_asm_offsets.obj' ],
              'action': [
                'python',
                '<(DEPTH)/third_party/libvpx/copy_obj.py',
                '-d', '<@(_outputs)',
                '-s', '<(PRODUCT_DIR)/obj/libvpx_asm_offsets_vpx_scale/vpx_scale_asm_offsets.obj',
                '-s', '<(ninja_obj_dir)/encoder/libvpx_asm_offsets_vpx_scale.vpx_scale_asm_offsets.obj',
                '-s', '<(PRODUCT_DIR)/obj/Source/WebKit/chromium/third_party/libvpx/<(libvpx_source)/vpx_scale/libvpx_asm_offsets_vpx_scale.vpx_scale_asm_offsets.obj',
                '-s', '<(qtwe_chromium_obj_dir)/third_party/libvpx/<(libvpx_source)/vpx_scale/libvpx_asm_offsets_vpx_scale.vpx_scale_asm_offsets.obj',
              ],
              'process_output_as_sources': 1,
            },
          ],
          'sources': [
            '<(INTERMEDIATE_DIR)/vpx_scale_asm_offsets.obj',
          ],
        }, {
          'variables': {
            'unpack_lib_search_path_list': [
              '-a', '<(PRODUCT_DIR)/libvpx_asm_offsets_vpx_scale.a',
              '-a', '<(LIB_DIR)/third_party/libvpx/libvpx_asm_offsets_vpx_scale.a',
              '-a', '<(LIB_DIR)/Source/WebKit/chromium/third_party/libvpx/libvpx_asm_offsets_vpx_scale.a',
              '-a', '<(lib_intermediate_name)',
            ],
            'unpack_lib_output_dir':'<(INTERMEDIATE_DIR)',
            'unpack_lib_name':'vpx_scale_asm_offsets.o'
          },
          'includes': ['unpack_lib_posix.gypi'],
         # Need this otherwise gyp won't run the rule on them.
          'sources': [
            '<(INTERMEDIATE_DIR)/vpx_scale_asm_offsets.o',
          ],
        }],
      ],
      'includes': ['obj_int_extract.gypi'],
    },
  ],
}
