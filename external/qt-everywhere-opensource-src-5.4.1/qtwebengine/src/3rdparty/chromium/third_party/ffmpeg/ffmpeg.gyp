# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# There's a couple key GYP variables that control how FFmpeg is built:
#   ffmpeg_branding
#     Controls whether we build the Chromium or Google Chrome version of
#     FFmpeg.  The Google Chrome version contains additional codecs.
#     Typical values are Chromium, Chrome, ChromiumOS, and ChromeOS.
#   build_ffmpegsumo
#     When set to zero will build Chromium against Chrome's FFmpeg headers, but
#     not build ffmpegsumo itself.  Users are expected to build and provide
#     their own version of ffmpegsumo.  Default value is 1.
#

{
  'target_defaults': {
    'variables': {
      # Since we are not often debugging FFmpeg, and performance is
      # unacceptable without optimization, freeze the optimizations to -O2.
      # If someone really wants -O1 , they can change these in their checkout.
      # If you want -O0, see the Gotchas in README.Chromium for why that
      # won't work.
      'release_optimize': '2',
      'debug_optimize': '2',
      'mac_debug_optimization': '2',
      # In addition to the above reasons, /Od optimization won't remove symbols
      # that are under "if (0)" style sections.  Which lead to link time errors
      # when for example it tries to link an ARM symbol on X86.
      'win_debug_Optimization': '2',
      # Run time checks are incompatible with any level of optimizations.
      'win_debug_RuntimeChecks': '0',
      'conditions': [
        ['OS == "win"', {
          # Setting the optimizations to 'speed' or to 'max' results in a lot of
          # unresolved symbols. The only supported mode is 'size' (see
          # crbug.com/264459).
          'optimize' :'size',
        }],
      ],
    },
  },
  'variables': {
    # Make sure asm_sources is always defined even if an arch doesn't have any
    # asm sources (e.g. mips or x86 with forcefully disabled asm).
    'asm_sources': [
    ],

    # Allow overriding the selection of which FFmpeg binaries to copy via an
    # environment variable.  Affects the ffmpeg_binaries target.
    'conditions': [
      ['target_arch == "arm" and arm_version == 7 and arm_neon == 1', {
        # Need a separate config for arm+neon vs arm
        'ffmpeg_config%': 'arm-neon',
      }, {
        'ffmpeg_config%': '<(target_arch)',
      }],
      ['OS == "mac" or OS == "win" or OS == "openbsd"', {
        'os_config%': '<(OS)',
      }, {  # all other Unix OS's use the linux config
        'conditions': [
          ['msan==1', {
            # MemorySanitizer doesn't like assembly code.
            'os_config%': 'linux-noasm',
          }, {
            'os_config%': 'linux',
          }]
        ],
      }],
      ['chromeos == 1', {
        'ffmpeg_branding%': '<(branding)OS',
      }, {  # otherwise, assume Chrome/Chromium.
        'ffmpeg_branding%': '<(branding)',
      }],
    ],

    'build_ffmpegsumo%': 1,

    # Locations for generated artifacts.
    'shared_generated_dir': '<(SHARED_INTERMEDIATE_DIR)/third_party/ffmpeg',

    # Stub generator script and signatures of all functions used by Chrome.
    'generate_stubs_script': '../../tools/generate_stubs/generate_stubs.py',
    'sig_files': ['chromium/ffmpegsumo.sigs'],
    'extra_header': 'chromium/ffmpeg_stub_headers.fragment',
  },
  'conditions': [
    ['target_arch != "arm" and os_config != "linux-noasm"', {
      'targets': [
        {
          'target_name': 'ffmpeg_yasm',
          'type': 'static_library',
          'includes': [
            'ffmpeg_generated.gypi',
            '../yasm/yasm_compile.gypi',
          ],
          'sources': [
            '<@(asm_sources)',
            # XCode doesn't want to link a pure assembly target and will fail
            # to link when it creates an empty file list.  So add a dummy file
            # keep the linker happy.  See http://crbug.com/157073
            'xcode_hack.c',
          ],
          'variables': {
            # Path to platform configuration files.
            'platform_config_root': 'chromium/config/<(ffmpeg_branding)/<(os_config)/<(ffmpeg_config)',

            'conditions': [
              ['target_arch == "ia32"', {
                'more_yasm_flags': [
                  '-DARCH_X86_32',
                 ],
              }, {
                'more_yasm_flags': [
                  '-DARCH_X86_64',
                ],
              }],
              ['OS == "mac"', {
                'more_yasm_flags': [
                  # Necessary to ensure symbols end up with a _ prefix; added by
                  # yasm_compile.gypi for Windows, but not Mac.
                  '-DPREFIX',
                ]
              }],
            ],
            'yasm_flags': [
              '-DPIC',
              '>@(more_yasm_flags)',
              '-I', '<(platform_config_root)',
              '-I', 'libavcodec/x86/',
              '-I', 'libavutil/x86/',
              '-I', '.',
              # Disable warnings, prevents log spam for things we won't fix.
              '-w',
              '-P', 'config.asm',
            ],
            'yasm_output_path': '<(shared_generated_dir)/yasm'
          },
        },
      ] # targets
    }], # arch != arm
    ['build_ffmpegsumo != 0', {
      'includes': [
        'ffmpeg_generated.gypi',
      ],
      'variables': {
        # Path to platform configuration files.
        'platform_config_root': 'chromium/config/<(ffmpeg_branding)/<(os_config)/<(ffmpeg_config)',
      },
      'targets': [
        {
          'target_name': 'ffmpegsumo',
          'type': 'loadable_module',
          'sources': [
            '<@(c_sources)',
            '<(platform_config_root)/config.h',
            '<(platform_config_root)/libavutil/avconfig.h',
          ],
          'include_dirs': [
            '<(platform_config_root)',
            '.',
          ],
          'defines': [
            'HAVE_AV_CONFIG_H',
            '_POSIX_C_SOURCE=200112',
            '_XOPEN_SOURCE=600',
            'PIC',
            # Disable deprecated features which generate spammy warnings.
            'FF_API_PIX_FMT_DESC=0',
            'FF_API_OLD_DECODE_AUDIO=0',
            'FF_API_DESTRUCT_PACKET=0',
            'FF_API_GET_BUFFER=0',
          ],
          'cflags': [
            '-fPIC',
            '-fomit-frame-pointer',
            # ffmpeg uses its own deprecated functions.
            '-Wno-deprecated-declarations',
          ],
          'conditions': [
            ['target_arch != "arm" and target_arch != "mipsel" and os_config != "linux-noasm"', {
              'dependencies': [
                'ffmpeg_yasm',
              ],
            }],
            ['clang == 1', {
              'xcode_settings': {
                'WARNING_CFLAGS': [
                  '-Wno-absolute-value',
                  # ffmpeg uses its own deprecated functions.
                  '-Wno-deprecated-declarations',
                  # ffmpeg doesn't care about pointer constness.
                  '-Wno-incompatible-pointer-types',
                  # ffmpeg doesn't follow usual parentheses conventions.
                  '-Wno-parentheses',
                  # ffmpeg doesn't care about pointer signedness.
                  '-Wno-pointer-sign',
                  # ffmpeg doesn't believe in exhaustive switch statements.
                  '-Wno-switch',
                ],
              },
              'cflags': [
                '-Wno-absolute-value',
                '-Wno-incompatible-pointer-types',
                '-Wno-logical-op-parentheses',
                '-Wno-parentheses',
                '-Wno-pointer-sign',
                '-Wno-switch',
                # Don't emit warnings for gcc -f flags clang doesn't implement.
                '-Qunused-arguments',
              ],
              'conditions': [
                ['ffmpeg_branding == "Chrome" or ffmpeg_branding == "ChromeOS"', {
                  'xcode_settings': {
                    'WARNING_CFLAGS': [
                      # Clang doesn't support __attribute__((flatten)),
                      # http://llvm.org/PR7559
                      # This is used in the h264 decoder.
                      '-Wno-attributes',
                    ],
                  },
                  'cflags': [
                    '-Wno-attributes',
                  ],
                }],
              ],
            }, {
              'cflags': [
                # gcc doesn't have flags for specific warnings, so disable them
                # all.
                '-w',
              ],
            }],
            ['target_arch == "ia32"', {
              # Turn off valgrind build option that breaks ffmpeg builds.
              'cflags!': [
                '-fno-omit-frame-pointer',
              ],
              'debug_extra_cflags!': [
                '-fno-omit-frame-pointer',
              ],
              'release_extra_cflags!': [
                '-fno-omit-frame-pointer',
              ],
            }],  # target_arch == "ia32"
            ['target_arch == "arm"', {
              # TODO(ihf): See the long comment in build_ffmpeg.sh
              # We want to be consistent with CrOS and have configured
              # ffmpeg for thumb. Protect yourself from -marm.
              'cflags!': [
                '-marm',
              ],
              'cflags': [
                '-mthumb',
                '-march=armv7-a',
                '-mtune=cortex-a8',
              ],
              # On arm we use gcc to compile the assembly.
              'sources': [
                '<@(asm_sources)',
              ],
              'conditions': [
                ['arm_neon == 0', {
                  'cflags': [
                    '-mfpu=vfpv3-d16',
                  ],
                }, {
                  'cflags': [
                    '-mfpu=neon',
                  ],
                }],
                ['arm_float_abi == "hard"', {
                  'cflags': [
                    '-DHAVE_VFP_ARGS=1'
                  ],
                }, {
                  'cflags': [
                    '-DHAVE_VFP_ARGS=0'
                  ],
                }],
              ],
            }],
            ['target_arch == "mipsel"', {
              'cflags': [
                '-mips32',
                '-EL -Wl,-EL',
              ],
            }],  # target_arch == "mipsel"
            ['os_posix == 1 and OS != "mac"', {
              'defines': [
                '_ISOC99_SOURCE',
                '_LARGEFILE_SOURCE',
                # BUG(ihf): ffmpeg compiles with this define. But according to
                # ajwong: I wouldn't change _FILE_OFFSET_BITS.  That's a scary change
                # cause it affects the default length of off_t, and fpos_t,
                # which can cause strange problems if the loading code doesn't
                # have it set and you start passing FILE*s or file descriptors
                # between symbol contexts.
                # '_FILE_OFFSET_BITS=64',
              ],
              'cflags': [
                '-std=c99',
                '-pthread',
                '-fno-math-errno',
                '-fno-signed-zeros',
                '-fno-tree-vectorize',
              ],
              'cflags!': [
                # Ensure the symbols are exported.
                #
                # TODO(ajwong): Manually tag the API that we use to be
                # exported.
                '-fvisibility=hidden',
              ],
              'link_settings': {
                'ldflags': [
                  '-Wl,-Bsymbolic',
                  '-L<(shared_generated_dir)',
                ],
                'libraries': [
                  '-lz',
                ],
              },
            }],  # os_posix == 1 and OS != "mac"
            ['OS == "openbsd"', {
              # OpenBSD's gcc (4.2.1) does not support this flag
              'cflags!': [
                '-fno-signed-zeros',
              ],
            }],
            ['OS == "mac"', {
              'defines': [
                '_DARWIN_C_SOURCE',
              ],
              'conditions': [
                ['mac_breakpad == 1', {
                  'variables': {
                    # A real .dSYM is needed for dump_syms to operate on.
                    'mac_real_dsym': 1,
                  },
                }],
                ['target_arch != "x64"', {
                  # -read_only_relocs cannot be used with x86_64
                  'xcode_settings': {
                    'OTHER_LDFLAGS': [
                      # This is needed because even though FFmpeg now builds
                      # with -fPIC, it's not quite a complete PIC build, only
                      # partial :( Thus we need to instruct the linker to allow
                      # relocations for read-only segments for this target to be
                      # able to generated the shared library on Mac.
                      #
                      # This makes Mark sad, but he's okay with it since it is
                      # isolated to this module. When Mark finds this in the
                      # future, and has forgotten this conversation, this
                      # comment should remind him that the world is still nice
                      # and butterflies still exist...as do rainbows, sunshine,
                      # tulips, etc., etc...but not kittens. Those went away
                      # with this flag.
                      '-Wl,-read_only_relocs,suppress',
                    ],
                  },
                }],
              ],
              'link_settings': {
                'libraries': [
                  '$(SDKROOT)/usr/lib/libz.dylib',
                ],
              },
              'xcode_settings': {
                'GCC_SYMBOLS_PRIVATE_EXTERN': 'NO',  # No -fvisibility=hidden
                'DYLIB_INSTALL_NAME_BASE': '@loader_path',
                'LIBRARY_SEARCH_PATHS': [
                  '<(shared_generated_dir)'
                ],
              },
            }],  # OS == "mac"
            ['OS == "win"', {
              'sources': [
                '<(shared_generated_dir)/ffmpegsumo.def',
              ],
              'include_dirs': [
                'chromium/include/win',
              ],
              # TODO(dalecurtis): We should fix these.  http://crbug.com/154421
              'msvs_disabled_warnings': [
                4996, 4018, 4090, 4305, 4133, 4146, 4554, 4028, 4334, 4101, 4102,
                4116, 4307, 4273, 4005, 4056, 4756,
              ],
              'conditions': [
                ['clang == 1 or (OS == "win" and (MSVS_VERSION == "2013" or MSVS_VERSION == "2013e"))', {
                  'defines': [
                    'inline=__inline',
                    'strtoll=_strtoi64',
                    '_ISOC99_SOURCE',
                    '_LARGEFILE_SOURCE',
                    'HAVE_AV_CONFIG_H',
                    'strtod=avpriv_strtod',
                    'snprintf=avpriv_snprintf',
                    '_snprintf=avpriv_snprintf',
                    'vsnprintf=avpriv_vsnprintf',
                  ],
                }],
                ['target_arch == "x64"', {
                  # TODO(wolenetz): We should fix this.  http://crbug.com/171009
                  'msvs_disabled_warnings' : [
                    4267
                  ],
                }],
              ],
              'msvs_settings': {
                # Ignore warnings about a local symbol being inefficiently imported,
                # upstream is working on a fix.
                'VCLinkerTool': {
                  'AdditionalOptions': ['/ignore:4049', '/ignore:4217'],
                }
              },
              'actions': [
                {
                  'action_name': 'generate_def',
                  'inputs': [
                    '<(generate_stubs_script)',
                    '<@(sig_files)',
                  ],
                  'outputs': [
                    '<(shared_generated_dir)/ffmpegsumo.def',
                  ],
                  'action': ['python',
                             '<(generate_stubs_script)',
                             '-i', '<(INTERMEDIATE_DIR)',
                             '-o', '<(shared_generated_dir)',
                             '-t', 'windows_def',
                             '-m', 'ffmpegsumo.dll',
                             '<@(_inputs)',
                  ],
                  'message': 'Generating FFmpeg export definitions',
                },
              ],
            }],
          ],
        },
      ],
    }],
  ],  # conditions
  'targets': [
    {
      'target_name': 'ffmpeg',
      'sources': [
        # Files needed for stub generation rules.
        '<@(sig_files)',
      ],
      'defines': [
        '__STDC_CONSTANT_MACROS',  # FFmpeg uses INT64_C.
      ],
      'hard_dependency': 1,

      # Do not fear the massive conditional blocks!  They do the following:
      #   1) Use the Window stub generator on Windows
      #   2) Else, use the POSIX stub generator on non-Windows
      'conditions': [
        ['OS == "win"', {
          'msvs_guid': 'D7A94F58-576A-45D9-A45F-EB87C63ABBB0',
          'variables': {
            'conditions': [
              ['target_arch == "x64"', {
                'outfile_type': 'windows_lib_x64',
              }, {  # else, generate x86 stub library
                'outfile_type': 'windows_lib',
              }],
            ],
            'output_dir': '<(PRODUCT_DIR)/lib',
            'intermediate_dir': '<(INTERMEDIATE_DIR)',
          },
          'type': 'none',
          'sources': [
            # Adds C99 types for Visual C++.
            'chromium/include/win/inttypes.h',
          ],
          'dependencies': [
            'ffmpegsumo',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(platform_config_root)',
              'chromium/include/win',
              '.',
            ],
            'link_settings': {
              'libraries': [
                '<(output_dir)/ffmpegsumo.lib',
              ],
              'msvs_settings': {
                'VCLinkerTool': {
                  'DelayLoadDLLs': [
                    'ffmpegsumo.dll',
                  ],
                },
              },
            },
          },
          'rules': [
            {
              'rule_name': 'generate_libs',
              'extension': 'sigs',
              'inputs': [
                '<(generate_stubs_script)',
                '<@(sig_files)',
              ],
              'outputs': [
                '<(output_dir)/<(RULE_INPUT_ROOT).lib',
              ],
              'action': ['python', '<(generate_stubs_script)',
                         '-i', '<(intermediate_dir)',
                         '-o', '<(output_dir)',
                         '-t', '<(outfile_type)',
                         '<@(RULE_INPUT_PATH)',
              ],
              'message': 'Generating FFmpeg import libraries',
            },
          ],
        }, {  # else OS != "win", use POSIX stub generator
          'variables': {
            'outfile_type': 'posix_stubs',
            'stubs_filename_root': 'ffmpeg_stubs',
            'project_path': 'third_party/ffmpeg',
            'intermediate_dir': '<(INTERMEDIATE_DIR)',
            'output_root': '<(SHARED_INTERMEDIATE_DIR)/ffmpeg',
            'platform_config_root': 'chromium/config/<(ffmpeg_branding)/<(os_config)/<(ffmpeg_config)'
          },
          'sources': [
            '<(extra_header)',
          ],
          'type': '<(component)',
          'include_dirs': [
            '<(output_root)',
            '../..',  # The chromium 'src' directory.
            '<(platform_config_root)',
            '.',
          ],
          'dependencies': [
            # Required for the logging done in the stubs generator.
            '../../base/base.gyp:base',
          ],
          'direct_dependent_settings': {
            'defines': [
              '__STDC_CONSTANT_MACROS',  # FFmpeg uses INT64_C.
            ],
            'include_dirs': [
              '<(output_root)',
              '../..',  # The chromium 'src' directory.
              '<(platform_config_root)',
              '.',
            ],
          },
          'actions': [
            {
              'action_name': 'generate_stubs',
              'inputs': [
                '<(generate_stubs_script)',
                '<(extra_header)',
                '<@(sig_files)',
              ],
              'outputs': [
                '<(intermediate_dir)/<(stubs_filename_root).cc',
                '<(output_root)/<(project_path)/<(stubs_filename_root).h',
              ],
              'action': ['python',
                         '<(generate_stubs_script)',
                         '-i', '<(intermediate_dir)',
                         '-o', '<(output_root)/<(project_path)',
                         '-t', '<(outfile_type)',
                         '-e', '<(extra_header)',
                         '-s', '<(stubs_filename_root)',
                         '-p', '<(project_path)',
                         '<@(_inputs)',
              ],
              'process_outputs_as_sources': 1,
              'message': 'Generating FFmpeg stubs for dynamic loading',
            },
          ],
          'conditions': [
            # Linux/Solaris need libdl for dlopen() and friends.
            ['OS == "linux" or OS == "solaris"', {
              'link_settings': {
                'libraries': [
                  '-ldl',
                ],
              },
            }],
            ['component == "shared_library"', {
              'cflags!': ['-fvisibility=hidden'],
              'xcode_settings': {
                'GCC_SYMBOLS_PRIVATE_EXTERN': 'NO',  # no -fvisibility=hidden
              },
            }],
            ['build_ffmpegsumo != 0', {
              'dependencies': [
                'ffmpegsumo',
              ],
            }],
          ],  # conditions
        }],
      ],  # conditions
    },
  ],  # targets
}
