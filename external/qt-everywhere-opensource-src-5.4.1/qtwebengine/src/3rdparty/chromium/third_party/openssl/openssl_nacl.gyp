# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'includes': [
    '../../native_client/build/untrusted.gypi',
  ],
  'targets': [
    {
      'target_name': 'openssl_nacl',
      'type': 'none',
      'variables': {
        'nlib_target': 'libopenssl_nacl.a',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_pnacl_newlib': 1,
        'defines!': [
          '_XOPEN_SOURCE=600',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/native_client_sdk/native_client_sdk_untrusted.gyp:nacl_io_untrusted',
      ],
      'includes': [
        # Include the auto-generated gypi file.
        'openssl.gypi'
      ],
      'sources': [
        '<@(openssl_common_sources)',
      ],
      'defines': [
        '<@(openssl_common_defines)',
        'MONOLITH',
        'NO_SYS_UN_H',
        'NO_SYSLOG',
        'OPENSSL_NO_ASM',
        'PURIFY',
        'TERMIOS',
        'SSIZE_MAX=INT_MAX',
      ],
      'defines!': [
        'TERMIO',
      ],
      'include_dirs': [
        '.',
        'openssl',
        'openssl/crypto',
        'openssl/crypto/asn1',
        'openssl/crypto/evp',
        'openssl/crypto/modes',
        'openssl/include',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'openssl/include',
        ],
      },
      'pnacl_compile_flags': [
        '-Wno-sometimes-uninitialized',
        '-Wno-unused-variable',
      ],
    },  # target openssl_nacl
  ],
}
