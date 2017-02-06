# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Definitions to be used when building stand-alone NaCl binaries.

{
  'variables': {
    # These flags are only included in the NaCl standalone build.
    # They won't get included when NaCl is built as part of another
    # project, such as Chrome.  We don't necessarily want to enable
    # these flags for Chrome source that is built as NaCl untrusted
    # code.
    'nacl_default_compile_flags': [
      '-Wundef',
      '-Wstrict-prototypes',
    ],

    # Override chromium_mac_pch and set it to 0 to suppress the use of
    # precompiled headers on the Mac.  Prefix header injection may still be
    # used, but prefix headers will not be precompiled.  This is useful when
    # using distcc to distribute a build to compile slaves that don't
    # share the same compiler executable as the system driving the compilation,
    # because precompiled headers rely on pointers into a specific compiler
    # executable's image.  Setting this to 0 is needed to use an experimental
    # Linux-Mac cross compiler distcc farm.
    'chromium_mac_pch%': 1,

    # Enable building with ASAN (Clang's -fsanitize=address option).
    # -fsanitize=address only works with clang, but asan=1 implies clang=1
    # See https://sites.google.com/a/chromium.org/dev/developers/testing/addresssanitizer
    'asan%': 0,

    # Set this to true when building with Clang.
    'clang%': 0,

    'conditions': [
      ['OS=="mac"', {
        # On Mac only clang is new enough to build the trusted code.
        'clang%': 1,

        'variables': {
          # mac_sdk_min must be kept in synch with mac_sdk_min in
          # chromium/src/build/common.gypi.
          'mac_sdk_min%': '10.10',
        },

        # The Mac SDK is set for iOS builds and passed through to Mac
        # sub-builds. This allows the Mac sub-build SDK in an iOS build to be
        # overridden from the command line the same way it is for a Mac build.
        'mac_sdk%': '<!(python <(DEPTH)/native_client/build/mac/find_sdk.py <(mac_sdk_min))',

      }],
      ['OS=="linux"', {
        'clang%': 1,
      }],
      # Set ARM float abi compilation flag.
      ['OS=="android"', {
        'arm_float_abi%': 'softfp',
      }, {
        'arm_float_abi%': 'hard',
      }],
      ['host_clang==1', {
        'host_cc': '<(make_clang_dir)/bin/clang',
        'host_cxx': '<(make_clang_dir)/bin/clang++',
      }],
      ['host_clang==0', {
        'host_cc': '<!(which gcc)',
        'host_cxx': '<!(which g++)',
      }],
    ],

    # Set to 1 to enable code coverage.  In addition to build changes
    # (e.g. extra CFLAGS), also creates a new target in the src/chrome
    # project file called "coverage".
    # Currently ignored on Windows.
    'coverage%': 0,

    # TODO(sgk): eliminate this if possible.
    # It would be nicer to support this via a setting in 'target_defaults'
    # in chrome/app/locales/locales.gypi overriding the setting in the
    # 'Debug' configuration in the 'target_defaults' dict below,
    # but that doesn't work as we'd like.
    'msvs_debug_link_incremental%': '2',

    # NOTE: adapted from them chrome common.gypi file for arm
    'arm_version%': 7,

    # Set Neon compilation flags (only meaningful if arm_version==7).
    'arm_neon%': 1,

    # Set Thumb compilation flags.
    'arm_thumb%': 0,

    # Set ARM fpu compilation flags (only meaningful if arm_version==7 and
    # arm_neon==0).
    'arm_fpu%': 'vfpv3',

    # Default MIPS arch variant. This is set in the conditions block
    # below for MIPS targets.
    'mips_arch_variant%': 'r2',

    'mac_deployment_target%': '10.6',

    'nacl_strict_warnings%': 1,

    # NOTE: end adapted from them chrome common.gypi file for arm

    'variables': {
      'variables': {
        'variables': {
          'conditions': [
            # Compute the architecture that we're building on.
            # This logic needs to be kept in sync with Chrome's common.gypi or
            # subtle build errors will result when integrating into Chrome.
            # Generally, a desync will result in NaCl libraries will be a
            # different bittage than Chrome.  The kicker is that builds that
            # explicitly specify target_arch will not notice this desync, (this
            # logic is overridden) but builds that do not specify target_arch will
            # break.  This can cause confusion because only some trybots break.
            ['OS=="win" or OS=="ios"', {
              'host_arch%': 'ia32',
            }, {
              # This handles the Linux platforms we generally deal with. Anything
              # else gets passed through, which probably won't work very well;
              # such hosts should pass an explicit target_arch to gyp.
              #
              # NOTE: currently only nacl is generating gyp files on an arm board.
              #    The arm.* -> arm substitution in chrome's common.gypi isn't
              #    appropriate in that context as we actually use target_arch==arm
              #    to mean x86 -> arm cross compile. When actually running on an
              #    arm board, we'll generate ia32 for now, so that the generation
              #    succeeds.
              'host_arch%':
                  '<!(echo "<!pymod_do_main(detect_nacl_host_arch)" | sed -e "s/arm.*/ia32/")',
            }],
          ]
        },
        'host_arch%': '<(host_arch)',

        'make_clang_dir%': 'third_party/llvm-build/Release+Asserts',

        # By default we build against a stable sysroot image to avoid
        # depending on the packages installed on the local machine. Set this
        # to 0 to build against locally installed headers and libraries (e.g.
        # if packaging for a linux distro)
        'use_sysroot%': 1,

        # Override branding to select the desired branding flavor.
        'branding%': 'Chromium',

        # Override buildtype to select the desired build flavor.
        # Dev - everyday build for development/testing
        # Official - release build (generally implies additional processing)
        # TODO(mmoss) Once 'buildtype' is fully supported (e.g. Windows gyp
        # conversion is done), some of the things which are now controlled by
        # 'branding', such as symbol generation, will need to be refactored
        # based on 'buildtype' (i.e. we don't care about saving symbols for
        # non-Official builds).
        'buildtype%': 'Dev',

        'conditions': [
          ['OS=="android"', { # Android target_arch defaults to ARM.
            'target_arch%': 'arm',
          }, {
            # By default, build for the architecture that we're building on.
            'target_arch%': '<(host_arch)',
          }],
        ],
      },
      # These come from the above variable scope.
      'branding%': '<(branding)',
      'buildtype%': '<(buildtype)',
      'host_arch%': '<(host_arch)',
      'target_arch%': '<(target_arch)',
      'use_sysroot%': '<(use_sysroot)',
      'make_clang_dir%': '<(make_clang_dir)',

      # If this is set clang is used as host compiler, but not as target
      # compiler. Always do this by default.
      'host_clang%': 1,

      # This variable is to allow us to build components as either static
      # libraries or dynamic shared libraries.
      'component%': 'static_library',

      'conditions': [
        # A flag for POSIX platforms
        ['OS=="win"', {
          'os_posix%': 0,
         }, {
          'os_posix%': 1,
        }],

        # The system root for linux builds.
        ['OS=="linux" and use_sysroot==1', {
          # sysroot needs to be an absolute path otherwise it generates
          # incorrect results when passed to pkg-config
          'conditions': [
            ['target_arch=="arm"', {
              'sysroot%': '<!(cd <(DEPTH) && pwd -P)/build/linux/debian_wheezy_arm-sysroot',
            }],
            ['target_arch=="x64"', {
              'sysroot%': '<!(cd <(DEPTH) && pwd -P)/build/linux/debian_wheezy_amd64-sysroot',
            }],
            ['target_arch=="ia32"', {
              'sysroot%': '<!(cd <(DEPTH) && pwd -P)/build/linux/debian_wheezy_i386-sysroot',
            }],
            ['target_arch=="mipsel"', {
              'sysroot%': '<!(cd <(DEPTH) && pwd -P)/build/linux/debian_wheezy_mips-sysroot',
            }],
          ],
        }, {
          'sysroot%': ''
        }], # OS=="linux" and use_sysroot==1
      ],
    },
    # These come from the above variable scope.
    'branding%': '<(branding)',
    'buildtype%': '<(buildtype)',
    'component%': '<(component)',
    'host_arch%': '<(host_arch)',
    'make_clang_dir%': '<(make_clang_dir)',
    'os_posix%': '<(os_posix)',
    'sysroot%': '<(sysroot)',
    'use_sysroot%': '<(use_sysroot)',
    'target_arch%': '<(target_arch)',
  },

  'target_defaults': {
    'conditions': [
      ['coverage!=0', {
        'conditions': [
          ['OS=="mac"', {
            'xcode_settings': {
              'GCC_INSTRUMENT_PROGRAM_FLOW_ARCS': 'YES',
              'GCC_GENERATE_TEST_COVERAGE_FILES': 'YES',
            },
            # Add -lgcov for executables, not for static_libraries.
            # This is a delayed conditional.
            'target_conditions': [
              ['_type=="executable"', {
                'xcode_settings': { 'OTHER_LDFLAGS': [ '-lgcov' ] },
              }],
            ],
          }],
          # Linux gyp (into scons) doesn't like target_conditions?
          # TODO(???): track down why 'target_conditions' doesn't work
          # on Linux gyp into scons like it does on Mac gyp into xcodeproj.
          ['OS=="linux"', {
            'cflags': [ '-ftest-coverage',
                        '-fprofile-arcs'],
            'link_settings': { 'libraries': [ '-lgcov' ] },
          }],
        ],
      # TODO(jrg): options for code coverage on Windows
      }],
      ['clang==1', {
        'cflags': [
          '-Wheader-hygiene',
          # Clang spots more unused functions.
          '-Wno-unused-function',
          # Don't die on dtoa code that uses a char as an array index.
          '-Wno-char-subscripts',
          # Especially needed for gtest macros using enum values from Mac
          # system headers.
          # TODO(pkasting): In C++11 this is legal, so this should be
          # removed when we change to that.  (This is also why we don't
          # bother fixing all these cases today.)
          '-Wno-unnamed-type-template-args',
          # Allow 'default' label in switch even when all enumeration
          # cases have been covered.
          '-Wno-covered-switch-default',
          # Allow C++11 extensions (for "override")
          '-Wno-c++11-extensions',
        ],
        'cflags!': [
          # Clang doesn't seem to know know this flag.
          '-mfpmath=sse',
        ],
      }],
    ],
  },

  'conditions': [
    ['OS=="linux" or OS=="android"', {
      'target_defaults': {
        # Enable -Werror by default, but put it in a variable so it can
        # be disabled in ~/.gyp/include.gypi on the valgrind builders.
        'variables': {
          'werror%': '-Werror',
        },
        'cflags': [
          '<(werror)',  # See note in standalone_flags.gyp about the
                        # werror variable.
          '-pthread',
          '-fno-exceptions',
          '-Wall', # TODO(bradnelson): why does this disappear?!?
        ],
        'conditions': [
          ['sysroot!=""', {
            'target_conditions': [
              ['_toolset=="target"', {
                'cflags': [
                  '--sysroot=<(sysroot)',
                ],
                'ldflags': [
                  '--sysroot=<(sysroot)',
                  '<!(<(DEPTH)/build/linux/sysroot_ld_path.sh <(sysroot))',
                ],
            }]]
          }],
          ['nacl_standalone==1 and OS=="linux"', {
            'cflags': ['-fPIC'],
          }],
          ['OS=="android"', {
            'target_conditions': [
              ['_toolset=="target"', {
                'defines': ['ANDROID'],
              }],
            ],
           }, {
            'link_settings': {
              'libraries': [
                '-lpthread',
              ],
            }
          }],
          ['nacl_strict_warnings==1', {
            # TODO(gregoryd): remove the condition when the issues in
            # Chrome code are fixed.
            'cflags': [
              # TODO(bradnelson): This was being masked by key naming
              # collisions. Track down the items blocking enabling this.
              #'-pedantic',
              '-Wextra',
              '-Wno-long-long',
              '-Wswitch-enum',
              '-Wsign-compare',
              '-Wundef',
              '-fdiagnostics-show-option',
            ],
            'cflags_c': [
              # Require defining functions as "foo(void)" rather than
              # "foo()" because, in C (but not C++), the latter
              # defines a function with unspecified arguments rather
              # than no arguments.
              '-Wstrict-prototypes',
            ],
          }],
          ['target_arch=="arm"', {
            'target_conditions': [
              ['_toolset=="target"', {
                'cflags': [
                  '-Wno-abi',
                  '-fno-exceptions',
                  '-Wall',
                  '-fPIC',
                ],
                # TODO(mcgrathr): This is copied from the arm section of
                # chromium/src/build/common.gypi, but these details really
                # should be more fully harmonized and shared.
                'conditions': [
                  ['arm_thumb==1', {
                    'cflags': [
                      '-mthumb',
                    ]
                  }],
                  ['clang==1 and OS!="android"', {
                    'cflags': [
                      '-target arm-linux-gnueabihf',
                    ],
                    'ldflags': [
                      '-target arm-linux-gnueabihf',
                    ],
                  }],
                  ['arm_version==7', {
                    'cflags': [
                    '-march=armv7-a',
                    '-mtune=cortex-a9',
                    '-mfloat-abi=<(arm_float_abi)',
                    ],
                    'conditions': [
                      ['arm_neon==1', {
                        'cflags': [ '-mfpu=neon', ],
                      }, {
                        'cflags': [ '-mfpu=<(arm_fpu)', ],
                      }]
                    ],
                  }],
                ],
              }],
            ],
          }],
          ['target_arch=="mipsel"', {
            'target_conditions': [
              ['_toolset=="target"', {
                # Copied from chromium build/common.gypi
                'conditions': [
                  ['mips_arch_variant=="r1"', {
                    'conditions': [
                      ['clang==1', {
                        'cflags': [ '-target mipsel-linux-gnu', '-march=mips32', ],
                        'ldflags': [ '-target mipsel-linux-gnu', ],
                      }, { # clang==0
                        'cflags': ['-mips32', '-Wa,-mips32', ],
                      }],
                    ],
                  }],
                  ['mips_arch_variant=="r2"', {
                    'conditions': [
                      ['clang==1', {
                        'cflags': [ '-target mipsel-linux-gnu', '-march=mips32r2', ],
                        'ldflags': [ '-target mipsel-linux-gnu', ],
                      }, { # clang==0
                        'cflags': ['-mips32r2', '-Wa,-mips32r2', ],
                      }],
                    ],
                  }]
                ]
              }],
            ],
          }],
          ['target_arch=="ia32" or target_arch=="x64"', {
            'conditions': [
              ['target_arch=="x64"', {
                'variables': {
                  'mbits_flag': '-m64',
                },
              }, {
                'variables': {
                  'mbits_flag': '-m32',
                }
              },],
            ],
            'target_conditions': [
              ['_toolset=="target"', {
                'asflags': [ '<(mbits_flag)', ],
                'cflags': [ '<(mbits_flag)', ],
                'ldflags': [ '<(mbits_flag)', ],
              }],
            ],
            'cflags': [
              '-fno-exceptions',
              '-Wall',
            ],
          }],
          ['host_arch=="ia32" or host_arch=="x64"', {
            'conditions': [
              ['host_arch=="x64"', {
                'variables': { 'host_mbits_flag': '-m64', },
              }, {
                'variables': { 'host_mbits_flag': '-m32', }
              }],
            ],
            'target_conditions': [
              ['_toolset=="host"', {
                'asflags': [ '<(host_mbits_flag)', ],
                'cflags': [ '<(host_mbits_flag)', ],
                'ldflags': [ '<(host_mbits_flag)', ],
              }],
            ],
          }],
        ],
        'cflags_cc': [
          '-fno-rtti',
          '-fno-threadsafe-statics',
        ],
        'ldflags': [
          '-pthread',
          '-Wl,-z,noexecstack',
        ],
        'configurations': {
          'Debug': {
            'variables': {
              'debug_optimize%': '0',
            },
            'defines': [
              '_DEBUG',
            ],
            'cflags': [
              '-O<(debug_optimize)',
              '-g',
              # One can use '-gstabs' to enable building the debugging
              # information in STABS format for breakpad's dumpsyms.
            ],
            'ldflags': [
              '-rdynamic',  # Allows backtrace to resolve symbols.
            ],
          },
          'Release': {
            'variables': {
              'release_optimize%': '2',
            },
            'cflags': [
              '-O<(release_optimize)',
              # Don't emit the GCC version ident directives, they just end up
              # in the .comment section taking up binary size.
              '-fno-ident',
              # Put data and code in their own sections, so that unused symbols
              # can be removed at link time with --gc-sections.
              '-fdata-sections',
              '-ffunction-sections',
            ],
            'conditions': [
              ['clang==1', {
                'cflags!': [
                  '-fno-ident',
                ],
              }],
            ],
          },
        },
      },
    }],
    ['OS=="mac"', {
      'target_defaults': {
        'variables': {
          # This should be 'mac_real_dsym%', but there seems to be a bug
          # with % in variables that are intended to be set to different
          # values in different targets, like this one.
          'mac_real_dsym': 0,  # Fake .dSYMs are fine in most cases.
        },
        'mac_bundle': 0,
        'xcode_settings': {
          'ALWAYS_SEARCH_USER_PATHS': 'NO',
          'GCC_C_LANGUAGE_STANDARD': 'gnu99',
          'GCC_CW_ASM_SYNTAX': 'NO',                # No -fasm-blocks
          'GCC_DYNAMIC_NO_PIC': 'NO',               # No -mdynamic-no-pic
                                                    # (Equivalent to -fPIC)
          'GCC_ENABLE_CPP_EXCEPTIONS': 'NO',        # -fno-exceptions
          'GCC_ENABLE_CPP_RTTI': 'NO',              # -fno-rtti
          'GCC_ENABLE_PASCAL_STRINGS': 'NO',        # No -mpascal-strings
          'GCC_INLINES_ARE_PRIVATE_EXTERN':
            'YES',  # -fvisibility-inlines-hidden
          'GCC_OBJC_CALL_CXX_CDTORS': 'YES',        # -fobjc-call-cxx-cdtors
          'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES',      # -fvisibility=hidden
          'GCC_THREADSAFE_STATICS': 'NO',           # -fno-threadsafe-statics
          'GCC_TREAT_WARNINGS_AS_ERRORS': 'YES',    # -Werror
          # Don't set GCC_VERSION so that we default to /usr/bin/gcc
          #'GCC_VERSION': '4.2',
          'GCC_WARN_ABOUT_MISSING_NEWLINE': 'YES',  # -Wnewline-eof
          'PREBINDING': 'NO',                       # No -Wl,-prebind
          'USE_HEADERMAP': 'NO',
          # TODO(bradnelson): -Werror ?!?
          'WARNING_CFLAGS': ['-Wall', '-Wendif-labels', '-Wno-long-long'],
          'conditions': [
            ['chromium_mac_pch', {'GCC_PRECOMPILE_PREFIX_HEADER': 'YES'},
                                 {'GCC_PRECOMPILE_PREFIX_HEADER': 'NO'}],
            ['clang==1', {
              'CC': '$(SOURCE_ROOT)/<(clang_dir)/clang',
              'LDPLUSPLUS': '$(SOURCE_ROOT)/<(clang_dir)/clang++',
              'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
              'WARNING_CFLAGS': [
                '-Wheader-hygiene',
                # Don't die on dtoa code that uses a char as an array index.
                # This is required solely for base/third_party/dmg_fp/dtoa.cc.
                '-Wno-char-subscripts',
                # Clang spots more unused functions.
                '-Wno-unused-function',
                # See comments on this flag higher up in this file.
                '-Wno-unnamed-type-template-args',
                # Allow 'default' label in switch even when all enumeration
                # cases have been covered.
                '-Wno-covered-switch-default',
                # Allow C++11 extensions (for "override")
                '-Wno-c++11-extensions',
                # TODO(thakis): Reenable once the one instance this warns on
                # is fixed.
                '-Wno-parentheses',
              ],
              # If part of the Chromium build, use the Chromium default.
              # Otherwise, when building standalone, use this.
              'MACOSX_DEPLOYMENT_TARGET': '<(mac_deployment_target)', # -mmacosx-version-min
              'SDKROOT': 'macosx<(mac_sdk)',            # -isysroot
            }],
          ],
        },
        'target_conditions': [
          ['_type!="static_library"', {
            'xcode_settings': {
              'OTHER_LDFLAGS': ['-Wl,-search_paths_first']},
          }],
          ['_mac_bundle', {
            'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
          }],
          ['_type=="executable"', {
            'target_conditions': [
              ['mac_real_dsym == 1', {
                # To get a real .dSYM bundle produced by dsymutil, set the
                # debug information format to dwarf-with-dsym.  Since
                # strip_from_xcode will not be used, set Xcode to do the
                # stripping as well.
                'configurations': {
                  'Release': {
                    'xcode_settings': {
                      'DEBUG_INFORMATION_FORMAT': 'dwarf-with-dsym',
                      'DEPLOYMENT_POSTPROCESSING': 'YES',
                      'STRIP_INSTALLED_PRODUCT': 'YES',
                    },
                  },
                },
              }, {  # mac_real_dsym != 1
                # To get a fast fake .dSYM bundle, use a post-build step to
                # produce the .dSYM and strip the executable.  strip_from_xcode
                # only operates in the Release configuration.
                'postbuilds': [
                  {
                    'variables': {
                      # Define strip_from_xcode in a variable ending in _path
                      # so that gyp understands it's a path and performs proper
                      # relativization during dict merging.
                      'strip_from_xcode_path':
                          'mac/strip_from_xcode',
                    },
                    'postbuild_name': 'Strip If Needed',
                    'action': ['<(strip_from_xcode_path)'],
                  },
                ],
              }],
            ],
          }],
        ],
        'conditions': [
          ['clang==1', {
            'variables': {
              'clang_dir':
                '<(DEPTH)/third_party/llvm-build/Release+Asserts/bin',
            },
          }],
          ['nacl_strict_warnings==1', {
            'xcode_settings': {
              # TODO(gregoryd): remove the condition when the issues in
              # Chrome code are fixed.
              'WARNING_CFLAGS': [
                '-pedantic',
                '-Wextra',
                '-Wno-long-long',
                '-Wswitch-enum',
                '-Wsign-compare',
                '-Wundef',
                '-fdiagnostics-show-option',
              ],
            },
          }],
          ['target_arch=="x64"', {
            'xcode_settings': {'ARCHS': ['x86_64']},
          }],
          ['target_arch=="ia32"', {
            'xcode_settings': {'ARCHS': ['i386']},
          }],
        ],
      },
    }],
    ['OS=="win"', {
      'target_defaults': {
        'defines': [
          '_WIN32_WINNT=0x0A00',
          'WINVER=0x0A00',
          # WIN32 is used by ppapi
          'WIN32',
          'NOMINMAX',
          '_CRT_RAND_S',
          'CERT_CHAIN_PARA_HAS_EXTRA_FIELDS',
          # WIN32_LEAN_AND_MEAN tells windows.h to omit obsolete and rarely
          # used #include files. This allows use of Winsock 2.0 which otherwise
          # would conflict with Winsock 1.x included by windows.h.
          'WIN32_LEAN_AND_MEAN',
          '_SECURE_ATL',
          '__STDC_LIMIT_MACROS=1',
          '_HAS_EXCEPTIONS=0',
        ],
        'msvs_cygwin_dirs': ['../third_party/cygwin'],
        # TODO(bsy) remove 4355 once cross-repo
        # NACL_ALLOW_THIS_IN_INITIALIZER_LIST changes go in.
        'msvs_disabled_warnings': [4355, 4396, 4503, 4800, 4819],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'MinimalRebuild': 'false',
            'BufferSecurityCheck': 'true',
            'EnableFunctionLevelLinking': 'true',
            'RuntimeTypeInfo': 'false',
            'WarnAsError': 'true',
            'DebugInformationFormat': '3',
            'ExceptionHandling': '0',
          },
          'VCLibrarianTool': {
            'AdditionalOptions': ['/ignore:4221'],
          },
          'VCLinkerTool': {
            'AdditionalDependencies': [
              'version.lib',
              'msimg32.lib',
              'ws2_32.lib',
              'usp10.lib',
              'psapi.lib',
              'dbghelp.lib',
            ],
            'DelayLoadDLLs': [
              'dbghelp.dll',
              'dwmapi.dll',
              'uxtheme.dll',
            ],
            'GenerateDebugInformation': 'true',
            'MapFileName': '$(OutDir)\\$(TargetName).map',
            'ImportLibrary': '$(OutDir)\\lib\\$(TargetName).lib',
            'TargetMachine': '1',
            'FixedBaseAddress': '1',
            # SubSystem values:
            #   0 == not set
            #   1 == /SUBSYSTEM:CONSOLE
            #   2 == /SUBSYSTEM:WINDOWS
            # Most of the executables we'll ever create are tests
            # and utilities with console output.
            'SubSystem': '1',
          },
          'VCMIDLTool': {
            'GenerateStublessProxies': 'true',
            'TypeLibraryName': '$(InputName).tlb',
            'OutputDirectory': '$(IntDir)',
            'HeaderFileName': '$(InputName).h',
            'DLLDataFileName': 'dlldata.c',
            'InterfaceIdentifierFileName': '$(InputName)_i.c',
            'ProxyFileName': '$(InputName)_p.c',
          },
          'VCResourceCompilerTool': {
            'Culture' : '1033',
            'AdditionalIncludeDirectories': ['<(DEPTH)'],
          },
        },
      },
    }],
    ['clang==1', {
      'make_global_settings': [
        ['CC', '<(make_clang_dir)/bin/clang'],
        ['CXX', '<(make_clang_dir)/bin/clang++'],
      ],
    }],
    ['OS=="linux" and target_arch=="mipsel" and clang==0', {
      'make_global_settings': [
        ['CC', '<!(which mipsel-linux-gnu-gcc)'],
        ['CXX', '<!(which mipsel-linux-gnu-g++)'],
        ['CC.host', '<(host_cc)'],
        ['CXX.host', '<(host_cxx)'],
      ],
    }],
    ['OS=="linux" and target_arch=="arm" and host_arch!="arm" and clang==0', {
      # Set default ARM cross compiling on linux.  These can be overridden
      # using CC/CXX/etc environment variables.
      'make_global_settings': [
        ['CC', '<!(which arm-linux-gnueabihf-gcc)'],
        ['CXX', '<!(which arm-linux-gnueabihf-g++)'],
        ['CC.host', '<(host_cc)'],
        ['CXX.host', '<(host_cxx)'],
      ],
    }],
    ['OS=="android"', {
      'includes': [
        'android_settings.gypi',
      ],
    }],  # OS=="android"
  ],
  'xcode_settings': {
    # The Xcode generator will look for an xcode_settings section at the root
    # of each dict and use it to apply settings on a file-wide basis.  Most
    # settings should not be here, they should be in target-specific
    # xcode_settings sections, or better yet, should use non-Xcode-specific
    # settings in target dicts.  SYMROOT is a special case, because many other
    # Xcode variables depend on it, including variables such as
    # PROJECT_DERIVED_FILE_DIR.  When a source group corresponding to something
    # like PROJECT_DERIVED_FILE_DIR is added to a project, in order for the
    # files to appear (when present) in the UI as actual files and not red
    # red "missing file" proxies, the correct path to PROJECT_DERIVED_FILE_DIR,
    # and therefore SYMROOT, needs to be set at the project level.
    'SYMROOT': '<(DEPTH)/xcodebuild',
  },
}
