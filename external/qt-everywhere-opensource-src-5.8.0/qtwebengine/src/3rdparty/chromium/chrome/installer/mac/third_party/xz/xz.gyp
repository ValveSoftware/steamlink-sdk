# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # Configuration. Ordinarily, all of these features are enabled, and
    # "small" is disabled. For Chromium's purposes, enable "small" and disable
    # nearly everything else to save about 36kB in the lzma_decompress
    # library, which will be distributed with updates. This also translates to
    # a 25kB savings in the compressed disk image that will contain the
    # update.
    'check_crc32': 1,
    'check_crc64': 0,
    'check_sha256': 0,
    'coder_lzma1': 0,
    'coder_lzma2': 1,
    'coder_delta': 0,
    'coder_bcj_arm': 0,
    'coder_bcj_armthumb': 0,
    'coder_bcj_ia64': 0,
    'coder_bcj_powerpc': 0,
    'coder_bcj_sparc': 0,
    'coder_bcj_x86': 1,
    'small': 1,

    'lzma_common_defines': [
      'HAVE_CONFIG_H',
    ],
    'lzma_decompress_defines': [
    ],
    'lzma_compress_defines': [
    ],

    'lzma_include_dirs': [
      'config/<(OS)',
      'xz/src/common',
      'xz/src/liblzma/api',
      'xz/src/liblzma/check',
      'xz/src/liblzma/common',
      'xz/src/liblzma/delta',
      'xz/src/liblzma/lz',
      'xz/src/liblzma/lzma',
      'xz/src/liblzma/rangecoder',
      'xz/src/liblzma/simple',
    ],

    'lzma_common_source_files': [
      'xz/src/common/tuklib_physmem.c',
      'xz/src/liblzma/check/check.c',
      'xz/src/liblzma/common/block_util.c',
      'xz/src/liblzma/common/common.c',
      'xz/src/liblzma/common/easy_preset.c',
      'xz/src/liblzma/common/filter_common.c',
      'xz/src/liblzma/common/hardware_physmem.c',
      'xz/src/liblzma/common/index.c',
      'xz/src/liblzma/common/stream_flags_common.c',
      'xz/src/liblzma/common/vli_size.c',
      'xz/src/liblzma/lzma/lzma_encoder_presets.c',
      'xz/src/liblzma/rangecoder/price_table.c',
    ],
    'lzma_decompress_source_files': [
      'xz/src/liblzma/common/alone_decoder.c',
      'xz/src/liblzma/common/auto_decoder.c',
      'xz/src/liblzma/common/block_buffer_decoder.c',
      'xz/src/liblzma/common/block_decoder.c',
      'xz/src/liblzma/common/block_header_decoder.c',
      'xz/src/liblzma/common/easy_decoder_memusage.c',
      'xz/src/liblzma/common/filter_buffer_decoder.c',
      'xz/src/liblzma/common/filter_decoder.c',
      'xz/src/liblzma/common/filter_flags_decoder.c',
      'xz/src/liblzma/common/index_decoder.c',
      'xz/src/liblzma/common/index_hash.c',
      'xz/src/liblzma/common/stream_buffer_decoder.c',
      'xz/src/liblzma/common/stream_decoder.c',
      'xz/src/liblzma/common/stream_flags_decoder.c',
      'xz/src/liblzma/common/vli_decoder.c',
      'xz/src/liblzma/lz/lz_decoder.c',
      'xz/src/liblzma/lzma/lzma_decoder.c',
    ],
    'lzma_compress_source_files': [
      'xz/src/liblzma/common/alone_encoder.c',
      'xz/src/liblzma/common/block_buffer_encoder.c',
      'xz/src/liblzma/common/block_encoder.c',
      'xz/src/liblzma/common/block_header_encoder.c',
      'xz/src/liblzma/common/easy_buffer_encoder.c',
      'xz/src/liblzma/common/easy_encoder.c',
      'xz/src/liblzma/common/easy_encoder_memusage.c',
      'xz/src/liblzma/common/filter_buffer_encoder.c',
      'xz/src/liblzma/common/filter_encoder.c',
      'xz/src/liblzma/common/filter_flags_encoder.c',
      'xz/src/liblzma/common/index_encoder.c',
      'xz/src/liblzma/common/stream_buffer_encoder.c',
      'xz/src/liblzma/common/stream_encoder.c',
      'xz/src/liblzma/common/stream_flags_encoder.c',
      'xz/src/liblzma/common/vli_encoder.c',
      'xz/src/liblzma/lz/lz_encoder.c',
      'xz/src/liblzma/lz/lz_encoder_mf.c',
      'xz/src/liblzma/lzma/lzma_encoder.c',
      'xz/src/liblzma/lzma/lzma_encoder_optimum_fast.c',
      'xz/src/liblzma/lzma/lzma_encoder_optimum_normal.c',
    ],

    'conditions': [
      ['small != 0', {
        'lzma_common_defines': [
          'HAVE_SMALL=1',
        ],
        'lzma_common_source_files': [
	  # CRC-32 must be compiled for header integrity checks, even if it is
	  # not enabled for payload integrity checks.
          'xz/src/liblzma/check/crc32_small.c',
        ],
      }, {
        'lzma_common_source_files': [
	  # CRC-32 must be compiled for header integrity checks, even if it is
	  # not enabled for payload integrity checks.
          #
          # Use the "fast" implementation instead of the "x86" one for
          # architecture independence. Both require the "table."
          'xz/src/liblzma/check/crc32_fast.c',
          'xz/src/liblzma/check/crc32_table.c',
          # 'xz/src/liblzma/check/crc32_x86.S',
        ],
        'lzma_compress_source_files': [
          'xz/src/liblzma/lzma/fastpos_table.c',
        ],
      }],
      ['check_crc32 != 0', {
        'lzma_common_defines': [
          'HAVE_CHECK_CRC32=1',
        ],
      }],
      ['check_crc64 != 0', {
        'lzma_common_defines': [
          'HAVE_CHECK_CRC64=1',
        ],
        'conditions': [
          ['small != 0', {
            'lzma_common_source_files': [
              'xz/src/liblzma/check/crc64_small.c',
            ],
          }, {
            'lzma_common_source_files': [
              # Use the "fast" implementation instead of the "x86" one for
              # architecture independence. Both require the "table."
              'xz/src/liblzma/check/crc64_fast.c',
              'xz/src/liblzma/check/crc64_table.c',
              # 'xz/src/liblzma/check/crc64_x86.S',
            ],
          }],
        ],
      }],
      ['check_sha256 != 0', {
        'lzma_common_defines': [
          'HAVE_CHECK_SHA256=1',
        ],
        'lzma_common_source_files': [
          'xz/src/liblzma/check/sha256.c',
        ],
      }],
      ['coder_lzma1 != 0', {
        'lzma_decompress_defines': [
          'HAVE_DECODER_LZMA1=1',
        ],
        'lzma_compress_defines': [
          'HAVE_ENCODER_LZMA1=1',
        ],
      }],
      ['coder_lzma2 != 0', {
        'lzma_decompress_defines': [
          'HAVE_DECODER_LZMA2=1',
        ],
        'lzma_compress_defines': [
          'HAVE_ENCODER_LZMA2=1',
        ],
        'lzma_decompress_source_files': [
          'xz/src/liblzma/lzma/lzma2_decoder.c',
        ],
        'lzma_compress_source_files': [
          'xz/src/liblzma/lzma/lzma2_encoder.c',
        ],
      }],
      ['coder_delta != 0', {
        'lzma_decompress_defines': [
          'HAVE_DECODER_DELTA=1',
        ],
        'lzma_compress_defines': [
          'HAVE_ENCODER_DELTA=1',
        ],
        'lzma_common_source_files': [
          'xz/src/liblzma/delta/delta_common.c',
        ],
        'lzma_decompress_source_files': [
          'xz/src/liblzma/delta/delta_decoder.c',
        ],
        'lzma_compress_source_files': [
          'xz/src/liblzma/delta/delta_encoder.c',
        ],
      }],
      ['coder_bcj_arm != 0', {
        'lzma_decompress_defines': [
          'HAVE_DECODER_ARM=1',
        ],
        'lzma_compress_defines': [
          'HAVE_ENCODER_ARM=1',
        ],
        'lzma_common_source_files': [
          'xz/src/liblzma/simple/arm.c',
        ],
      }],
      ['coder_bcj_armthumb != 0', {
        'lzma_decompress_defines': [
          'HAVE_DECODER_ARMTHUMB=1',
        ],
        'lzma_compress_defines': [
          'HAVE_ENCODER_ARMTHUMB=1',
        ],
        'lzma_common_source_files': [
          'xz/src/liblzma/simple/armthumb.c',
        ],
      }],
      ['coder_bcj_ia64 != 0', {
        'lzma_decompress_defines': [
          'HAVE_DECODER_IA64=1',
        ],
        'lzma_compress_defines': [
          'HAVE_ENCODER_IA64=1',
        ],
        'lzma_common_source_files': [
          'xz/src/liblzma/simple/ia64.c',
        ],
      }],
      ['coder_bcj_powerpc != 0', {
        'lzma_decompress_defines': [
          'HAVE_DECODER_POWERPC=1',
        ],
        'lzma_compress_defines': [
          'HAVE_ENCODER_POWERPC=1',
        ],
        'lzma_common_source_files': [
          'xz/src/liblzma/simple/powerpc.c',
        ],
      }],
      ['coder_bcj_sparc != 0', {
        'lzma_decompress_defines': [
          'HAVE_DECODER_SPARC=1',
        ],
        'lzma_compress_defines': [
          'HAVE_ENCODER_SPARC=1',
        ],
        'lzma_common_source_files': [
          'xz/src/liblzma/simple/sparc.c',
        ],
      }],
      ['coder_bcj_x86 != 0', {
        'lzma_decompress_defines': [
          'HAVE_DECODER_X86=1',
        ],
        'lzma_compress_defines': [
          'HAVE_ENCODER_X86=1',
        ],
        'lzma_common_source_files': [
          'xz/src/liblzma/simple/x86.c',
        ],
      }],
      ['coder_bcj_arm != 0 or coder_bcj_armthumb != 0 or coder_bcj_ia64 != 0 or coder_bcj_powerpc != 0 or coder_bcj_sparc != 0 or coder_bcj_x86 != 0', {
        'lzma_common_source_files': [
          'xz/src/liblzma/simple/simple_coder.c',
        ],
        'lzma_decompress_source_files': [
          'xz/src/liblzma/simple/simple_decoder.c',
        ],
        'lzma_compress_source_files': [
          'xz/src/liblzma/simple/simple_encoder.c',
        ],
      }],
    ],  # conditions
  },  # variables

  'targets': [
    {
      'target_name': 'lzma',
      'type': 'static_library',
      'defines': [
        '<@(lzma_common_defines)',
        '<@(lzma_decompress_defines)',
        '<@(lzma_compress_defines)',
      ],
      'include_dirs': [
        '<@(lzma_include_dirs)',
      ],
      'sources': [
        '<@(lzma_common_source_files)',
        '<@(lzma_decompress_source_files)',
        '<@(lzma_compress_source_files)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'xz/src/liblzma/api',
        ],
      },
    },

    {
      'target_name': 'lzma_decompress',

      # Always build as a shared library to promote code sharing. This library
      # is distributed in Google Chrome update disk images and is used by more
      # than one tool.
      'type': 'shared_library',

      'xcode_settings': {
        # The library will reside in the same directory as executables linked
        # against it.
        'DYLIB_INSTALL_NAME_BASE': '@loader_path',

        # Trim the list of exported symbols to just those needed by
        # dependents. This limits the set of "roots" seen during dead-code
        # stripping and reduces the size of the library.
        'EXPORTED_SYMBOLS_FILE': 'lzma_decompress.export',
      },

      'defines': [
        '<@(lzma_common_defines)',
        '<@(lzma_decompress_defines)',
      ],
      'include_dirs': [
        '<@(lzma_include_dirs)',
      ],
      'sources': [
        '<@(lzma_common_source_files)',
        '<@(lzma_decompress_source_files)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'xz/src/liblzma/api',
        ],
      },
      'configurations': {
        'Release': {
          'xcode_settings': {
            # Use -Os to minimize the size of the installer tools.
            'GCC_OPTIMIZATION_LEVEL': 's',
          },
        },
      },
    },

    {
      'target_name': 'xz',
      'type': 'executable',
      'dependencies': [
        'lzma',
      ],
      'defines': [
        '<@(lzma_common_defines)',
        '<@(lzma_decompress_defines)',
        '<@(lzma_compress_defines)',
      ],
      'include_dirs': [
        'config/<(OS)',
        'xz/src/common',
      ],
      'sources': [
        'xz/src/common/tuklib_cpucores.c',
        'xz/src/common/tuklib_exit.c',
        'xz/src/common/tuklib_mbstr_fw.c',
        'xz/src/common/tuklib_mbstr_width.c',
        'xz/src/common/tuklib_open_stdxxx.c',
        'xz/src/common/tuklib_progname.c',
        'xz/src/xz/args.c',
        'xz/src/xz/coder.c',
        'xz/src/xz/file_io.c',
        'xz/src/xz/hardware.c',
        'xz/src/xz/list.c',
        'xz/src/xz/main.c',
        'xz/src/xz/message.c',
        'xz/src/xz/options.c',
        'xz/src/xz/signals.c',
        'xz/src/xz/suffix.c',
        'xz/src/xz/util.c',
      ],
    },

    {
      'target_name': 'xzdec',
      'type': 'executable',
      'dependencies': [
        'lzma_decompress',
      ],
      'defines': [
        '<@(lzma_common_defines)',
        '<@(lzma_decompress_defines)',
      ],
      'include_dirs': [
        'config/<(OS)',
        'xz/src/common',
      ],
      'sources': [
        'xz/src/common/tuklib_exit.c',
        'xz/src/common/tuklib_progname.c',
        'xz/src/xzdec/xzdec.c',
      ],
      'configurations': {
        'Release': {
          'xcode_settings': {
            # Use -Os to minimize the size of the installer tools.
            'GCC_OPTIMIZATION_LEVEL': 's',
          },
        },
      },
    },
  ],
}
