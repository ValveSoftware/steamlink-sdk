# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../native_client/build/untrusted.gypi',
    'net.gypi',
  ],
  'targets': [
    {
      'target_name': 'net_nacl',
      'type': 'none',
      'variables': {
        'nacl_untrusted_build': 1,
        'nlib_target': 'libnet_nacl.a',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_pnacl_newlib': 1,
      },
      'dependencies': [
        '../crypto/crypto_nacl.gyp:crypto_nacl',
        '../native_client_sdk/native_client_sdk_untrusted.gyp:nacl_io_untrusted',
        '../third_party/boringssl/boringssl_nacl.gyp:boringssl_nacl',
        '../third_party/protobuf/protobuf.gyp:protobuf_lite',
        '../url/url_nacl.gyp:url_nacl',
        'net.gyp:net_derived_sources',
        'net.gyp:net_quic_proto',
        'net.gyp:net_resources',
      ],
      'defines': [
        'NET_IMPLEMENTATION',
      ],
      'pnacl_compile_flags': [
        '-Wno-bind-to-temporary-copy',
      ],
      'sources': [
        '<@(net_nacl_common_sources)',
      ],
    },
  ],
}
