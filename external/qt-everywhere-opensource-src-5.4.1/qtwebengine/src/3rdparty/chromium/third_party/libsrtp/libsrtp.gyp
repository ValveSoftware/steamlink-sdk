# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'use_system_libsrtp%': 0,
  },
  'target_defaults': {
    'defines': [
      'HAVE_STDLIB_H',
      'HAVE_STRING_H',
    ],
    'include_dirs': [
      './config',
      'srtp/include',
      'srtp/crypto/include',
    ],
    'conditions': [
      ['os_posix==1', {
        'defines': [
          'HAVE_INT16_T',
          'HAVE_INT32_T',
          'HAVE_INT8_T',
          'HAVE_UINT16_T',
          'HAVE_UINT32_T',
          'HAVE_UINT64_T',
          'HAVE_UINT8_T',
          'HAVE_STDINT_H',
          'HAVE_INTTYPES_H',
          'HAVE_NETINET_IN_H',
          'INLINE=inline',
        ],
      }],
      ['OS=="win"', {
        'defines': [
          'INLINE=__inline',
          'HAVE_BYTESWAP_METHODS_H',
          # All Windows architectures are this way.
          'SIZEOF_UNSIGNED_LONG=4',
          'SIZEOF_UNSIGNED_LONG_LONG=8',
         ],
      }],
      ['target_arch=="x64" or target_arch=="ia32"', {
        'defines': [
          'CPU_CISC',
        ],
      }],
      ['target_arch=="arm" or target_arch=="armv7" or target_arch=="arm64"', {
        'defines': [
          # TODO(leozwang): CPU_RISC doesn't work properly on android/arm
          # platform for unknown reasons, need to investigate the root cause
          # of it. CPU_RISC is used for optimization only, and CPU_CISC should
          # just work just fine, it has been tested on android/arm with srtp
          # test applications and libjingle.
          'CPU_CISC',
        ],
      }],
      ['target_arch=="mipsel"', {
        'defines': [
          'CPU_RISC',
        ],
      }],
    ],
    'direct_dependent_settings': {
      'include_dirs': [
        './config',
        'srtp/include',
        'srtp/crypto/include',
      ],
      'conditions': [
        ['os_posix==1', {
          'defines': [
            'HAVE_INT16_T',
            'HAVE_INT32_T',
            'HAVE_INT8_T',
            'HAVE_UINT16_T',
            'HAVE_UINT32_T',
            'HAVE_UINT64_T',
            'HAVE_UINT8_T',
            'HAVE_STDINT_H',
            'HAVE_INTTYPES_H',
            'HAVE_NETINET_IN_H',
            'INLINE=inline',
          ],
        }],
        ['OS=="win"', {
          'defines': [
            'INLINE=__inline',
            'HAVE_BYTESWAP_METHODS_H',
            # All Windows architectures are this way.
            'SIZEOF_UNSIGNED_LONG=4',
            'SIZEOF_UNSIGNED_LONG_LONG=8',
           ],
        }],
        ['target_arch=="x64" or target_arch=="ia32"', {
          'defines': [
            'CPU_CISC',
          ],
        }],
        ['target_arch=="mipsel"', {
          'defines': [
            'CPU_RISC',
          ],
        }],
      ],
    },
  },
  'conditions': [
    ['use_system_libsrtp==0', {
      'targets': [
        {
          'target_name': 'libsrtp',
          'type': 'static_library',
          'sources': [
            # includes
            'srtp/include/ekt.h',
            'srtp/include/getopt_s.h',
            'srtp/include/rtp.h',
            'srtp/include/rtp_priv.h',
            'srtp/include/srtp.h',
            'srtp/include/srtp_priv.h',
            'srtp/include/ut_sim.h',

            # headers
            'srtp/crypto/include/aes_cbc.h',
            'srtp/crypto/include/aes.h',
            'srtp/crypto/include/aes_icm.h',
            'srtp/crypto/include/alloc.h',
            'srtp/crypto/include/auth.h',
            'srtp/crypto/include/cipher.h',
            'srtp/crypto/include/cryptoalg.h',
            'srtp/crypto/include/crypto.h',
            'srtp/crypto/include/crypto_kernel.h',
            'srtp/crypto/include/crypto_math.h',
            'srtp/crypto/include/crypto_types.h',
            'srtp/crypto/include/datatypes.h',
            'srtp/crypto/include/err.h',
            'srtp/crypto/include/gf2_8.h',
            'srtp/crypto/include/hmac.h',
            'srtp/crypto/include/integers.h',
            'srtp/crypto/include/kernel_compat.h',
            'srtp/crypto/include/key.h',
            'srtp/crypto/include/null_auth.h',
            'srtp/crypto/include/null_cipher.h',
            'srtp/crypto/include/prng.h',
            'srtp/crypto/include/rand_source.h',
            'srtp/crypto/include/rdb.h',
            'srtp/crypto/include/rdbx.h',
            'srtp/crypto/include/sha1.h',
            'srtp/crypto/include/stat.h',
            'srtp/crypto/include/xfm.h',

            # sources
            'srtp/srtp/ekt.c',
            'srtp/srtp/srtp.c',

            'srtp/crypto/cipher/aes.c',
            'srtp/crypto/cipher/aes_cbc.c',
            'srtp/crypto/cipher/aes_icm.c',
            'srtp/crypto/cipher/cipher.c',
            'srtp/crypto/cipher/null_cipher.c',
            'srtp/crypto/hash/auth.c',
            'srtp/crypto/hash/hmac.c',
            'srtp/crypto/hash/null_auth.c',
            'srtp/crypto/hash/sha1.c',
            'srtp/crypto/kernel/alloc.c',
            'srtp/crypto/kernel/crypto_kernel.c',
            'srtp/crypto/kernel/err.c',
            'srtp/crypto/kernel/key.c',
            'srtp/crypto/math/datatypes.c',
            'srtp/crypto/math/gf2_8.c',
            'srtp/crypto/math/stat.c',
            'srtp/crypto/replay/rdb.c',
            'srtp/crypto/replay/rdbx.c',
            'srtp/crypto/replay/ut_sim.c',
            'srtp/crypto/rng/ctr_prng.c',
            'srtp/crypto/rng/prng.c',
            'srtp/crypto/rng/rand_source.c',
          ],
        }, # target libsrtp
        {
          'target_name': 'rdbx_driver',
          'type': 'executable',
          'dependencies': [
            'libsrtp',
          ],
          'sources': [
            'srtp/include/getopt_s.h',
            'srtp/test/getopt_s.c',
            'srtp/test/rdbx_driver.c',
          ],
        },
        {
          'target_name': 'srtp_driver',
          'type': 'executable',
          'dependencies': [
            'libsrtp',
          ],
          'sources': [
            'srtp/include/getopt_s.h',
            'srtp/include/srtp_priv.h',
            'srtp/test/getopt_s.c',
            'srtp/test/srtp_driver.c',
          ],
        },
        {
          'target_name': 'roc_driver',
          'type': 'executable',
          'dependencies': [
            'libsrtp',
          ],
          'sources': [
            'srtp/crypto/include/rdbx.h',
            'srtp/include/ut_sim.h',
            'srtp/test/roc_driver.c',
          ],
        },
        {
          'target_name': 'replay_driver',
          'type': 'executable',
          'dependencies': [
            'libsrtp',
          ],
          'sources': [
            'srtp/crypto/include/rdbx.h',
            'srtp/include/ut_sim.h',
            'srtp/test/replay_driver.c',
          ],
        },
        {
          'target_name': 'rtpw',
          'type': 'executable',
          'dependencies': [
            'libsrtp',
          ],
          'sources': [
            'srtp/include/getopt_s.h',
            'srtp/include/rtp.h',
            'srtp/include/srtp.h',
            'srtp/crypto/include/datatypes.h',
            'srtp/test/getopt_s.c',
            'srtp/test/rtp.c',
            'srtp/test/rtpw.c',
          ],
          'conditions': [
            ['OS=="android"', {
              'defines': [
                'HAVE_SYS_SOCKET_H',
              ],
            }],
          ],
        },
        {
          'target_name': 'srtp_test_cipher_driver',
          'type': 'executable',
          'dependencies': [
            'libsrtp',
          ],
          'sources': [
            'srtp/crypto/test/cipher_driver.c',
          ],
        },
        {
          'target_name': 'srtp_test_datatypes_driver',
          'type': 'executable',
          'dependencies': [
            'libsrtp',
          ],
          'sources': [
            'srtp/crypto/test/datatypes_driver.c',
          ],
        },
        {
          'target_name': 'srtp_test_stat_driver',
          'type': 'executable',
          'dependencies': [
            'libsrtp',
          ],
          'sources': [
            'srtp/crypto/test/stat_driver.c',
          ],
        },
        {
          'target_name': 'srtp_test_sha1_driver',
          'type': 'executable',
          'dependencies': [
            'libsrtp',
          ],
          'sources': [
            'srtp/crypto/test/sha1_driver.c',
          ],
        },
        {
          'target_name': 'srtp_test_kernel_driver',
          'type': 'executable',
          'dependencies': [
            'libsrtp',
          ],
          'sources': [
            'srtp/crypto/test/kernel_driver.c',
          ],
        },
        {
          'target_name': 'srtp_test_aes_calc',
          'type': 'executable',
          'dependencies': [
            'libsrtp',
          ],
          'sources': [
            'srtp/crypto/test/aes_calc.c',
          ],
        },
        {
          'target_name': 'srtp_test_rand_gen',
          'type': 'executable',
          'dependencies': [
            'libsrtp',
          ],
          'sources': [
            'srtp/crypto/test/rand_gen.c',
          ],
        },
        {
          'target_name': 'srtp_test_env',
          'type': 'executable',
          'dependencies': [
            'libsrtp',
          ],
          'sources': [
            'srtp/crypto/test/env.c',
          ],
        },
        {
          'target_name': 'srtp_runtest',
          'type': 'none',
          'dependencies': [
            'rdbx_driver',
            'srtp_driver',
            'roc_driver',
            'replay_driver',
            'rtpw',
            'srtp_test_cipher_driver',
            'srtp_test_datatypes_driver',
            'srtp_test_stat_driver',
            'srtp_test_sha1_driver',
            'srtp_test_kernel_driver',
            'srtp_test_aes_calc',
            'srtp_test_rand_gen',
            'srtp_test_env',
          ],
        },
      ], # targets
    }, { # use_system_libsrtp==1
      'targets': [
        {
          'target_name': 'libsrtp',
          'type': 'none',
          'direct_dependent_settings': {
            'defines': [
              'USE_SYSTEM_LIBSRTP',
            ],
            'include_dirs': [
              '/usr/include/srtp',
            ],
          },
          'link_settings': {
            'libraries': [
              '-lsrtp',
            ],
          },
        }, # target libsrtp
      ], # targets
    }],
  ],
}
