# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../native_client/build/untrusted.gypi',
    'jingle.gypi',
  ],
  'targets': [
    {
      'target_name': 'jingle_glue_nacl',
      'type': 'none',
      'variables': {
        'nacl_untrusted_build': 1,
        'nlib_target': 'libjingle_glue_nacl.a',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_pnacl_newlib': 1,
      },
      'sources': [
        '<@(jingle_glue_sources)',
      ],
      'sources!': [
        'glue/chrome_async_socket.cc',
        'glue/proxy_resolving_client_socket.cc',
        'glue/xmpp_client_socket_factory.cc',
      ],
      'dependencies': [
        '../base/base_nacl.gyp:base_nacl',
        '../native_client/tools.gyp:prep_toolchain',
        '../net/net_nacl.gyp:net_nacl',
        '../third_party/libjingle/libjingle_nacl.gyp:libjingle_nacl',
      ],
      'export_dependent_settings': [
        '../third_party/libjingle/libjingle_nacl.gyp:libjingle_nacl',
      ],
    }
  ]
}
