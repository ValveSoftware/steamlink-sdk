# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'use_system_libvpx%': 0,
  },
  'conditions' : [
    ['use_system_libvpx == 0', {
  'variables': {
    'libvpx_build_vp9%': 1,
    'libvpx_source%': 'source/libvpx',
    # Disable LTO for neon targets
    # crbug.com/408997
    'use_lto%': 0,
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
    'conditions': [
      ['target_arch=="arm" and clang==1', {
        # TODO(hans) Enable integrated-as (crbug.com/124610).
        'cflags': [ '-fno-integrated-as' ],
        'conditions': [
          ['OS == "android"', {
            # Else /usr/bin/as gets picked up.
            'cflags': [ '-B<(android_toolchain)' ],
          }],
        ],
      }],
    ],
    'target_conditions': [
      ['<(libvpx_build_vp9)==0', {
        'sources/': [ ['exclude', '(^|/)vp9/'], ],
      }],
    ],
    'variables': {
      'conditions': [
        ['OS=="win"', {
          'optimize' :'max',
        }],
      ],
      'clang_warning_flags': [
        # libvpx heavily relies on implicit enum casting.
        '-Wno-conversion',
        # libvpx does `if ((a == b))` in some places.
        '-Wno-parentheses-equality',
        # libvpx has many static functions in header, which trigger this warning
        '-Wno-unused-function',
      ],
      'clang_warning_flags_unset': [
        # libvpx does assert(!"foo"); in some places.
        '-Wstring-conversion',
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
    [ '(target_arch != "arm" and target_arch != "armv7") and \
       (target_arch != "mipsel" and target_arch != "mips64el")', {
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
            ],
            # yasm only gets the flags we define. It doesn't inherit any of the
            # really useful defines that come with a gcc invocation. In this
            # case, we rely on __ANDROID__ to set 'rand' to 'lrand48'.
            # Previously we used the builtin _rand but that's gone away.
            'conditions': [
              ['OS=="android"', {
                'yasm_flags': [
                  '-D', '__ANDROID__',
                ],
              }],
            ],
          },
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
                'libvpx_intrinsics_sse4_1',
                'libvpx_intrinsics_avx',
                'libvpx_intrinsics_avx2',
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
                  ],
                  'dependencies': [
                    'libvpx_intrinsics_mmx',
                    'libvpx_intrinsics_sse2',
                    # Currently no sse3 intrinsic functions
                    #'libvpx_intrinsics_sse3',
                    'libvpx_intrinsics_ssse3',
                    'libvpx_intrinsics_sse4_1',
                    'libvpx_intrinsics_avx',
                    'libvpx_intrinsics_avx2',
                  ],
                }],
              ],
            }],
          ],
        },
      ],
    },
    ],
    # 'libvpx' target for mipsel and mips64el builds.
    [ 'target_arch=="mipsel" or target_arch=="mips64el"', {
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

          'includes': [ 'ads2gas.gypi', ],

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
          'conditions': [
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
              'dependencies': [
                '../../build/android/ndk.gyp:cpu_features',
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
  }, #  'use_system_libvpx == 0'
  {
  'targets': [
    {
      'target_name': 'libvpx',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(<(pkg-config) --cflags vpx)',
        ],
      },
      'variables': {
        'headers_root_path': 'source/libvpx',
        'header_filenames': [
          'vpx/vp8.h',
          'vpx/vp8cx.h',
          'vpx/vp8dx.h',
          'vpx/vpx_codec.h',
          'vpx/vpx_codec_impl_bottom.h',
          'vpx/vpx_codec_impl_top.h',
          'vpx/vpx_decoder.h',
          'vpx/vpx_encoder.h',
          'vpx/vpx_frame_buffer.h',
          'vpx/vpx_image.h',
          'vpx/vpx_integer.h',
        ],
      },
      'includes': [
        '../../build/shim_headers.gypi',
      ],
      'link_settings': {
        'ldflags': [
          '<!@(<(pkg-config) --libs-only-L --libs-only-other vpx)',
        ],
        'libraries': [
          '<!@(<(pkg-config) --libs-only-l vpx)',
        ],
      },
    },
  ],
  },
  ]]
}
