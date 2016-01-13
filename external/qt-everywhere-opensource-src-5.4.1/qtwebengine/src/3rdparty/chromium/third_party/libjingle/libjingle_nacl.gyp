# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'libjingle_source': "source",
  },
  'includes': [
    '../../native_client/build/untrusted.gypi',
  ],
  'targets': [
    {
      'target_name': 'libjingle_nacl',
      'type': 'none',
      'variables': {
        'nlib_target': 'libjingle_nacl.a',
        'nacl_untrusted_build': 1,
        'build_glibc': 0,
        'build_newlib': 0,
        'build_pnacl_newlib': 1,
        'use_openssl': 1,
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/native_client_sdk/native_client_sdk_untrusted.gyp:nacl_io_untrusted',
        '<(DEPTH)/third_party/expat/expat_nacl.gyp:expat_nacl',
        '<(DEPTH)/third_party/openssl/openssl_nacl.gyp:openssl_nacl',
        'libjingle_p2p_constants_nacl',
      ],
      'defines': [
        'EXPAT_RELATIVE_PATH',
        'FEATURE_ENABLE_SSL',
        'GTEST_RELATIVE_PATH',
        'HAVE_OPENSSL_SSL_H',
        'NO_MAIN_THREAD_WRAPPING',
        'NO_SOUND_SYSTEM',
        'POSIX',
        'SRTP_RELATIVE_PATH',
        'SSL_USE_OPENSSL',
        'USE_WEBRTC_DEV_BRANCH',
        'timezone=_timezone',
      ],
      'configurations': {
        'Debug': {
          'defines': [
            # TODO(sergeyu): Fix libjingle to use NDEBUG instead of
            # _DEBUG and remove this define. See below as well.
            '_DEBUG',
          ],
        }
      },
      'include_dirs': [
        '../testing/gtest/include',
        './<(libjingle_source)',
      ],
      'includes': ['libjingle_common.gypi', ],
      'sources!': [
        # Compiled as part of libjingle_p2p_constants_nacl.
        '<(libjingle_source)/talk/p2p/base/constants.cc',
        '<(libjingle_source)/talk/p2p/base/constants.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          './overrides',
          './<(libjingle_source)',
          '../../third_party/webrtc/overrides',
          '../../testing/gtest/include',
          '../../third_party',
          '../../third_party/webrtc',
        ],
        'defines': [
          'EXPAT_RELATIVE_PATH',
          'FEATURE_ENABLE_SSL',
          'GTEST_RELATIVE_PATH',
          'NO_MAIN_THREAD_WRAPPING',
          'NO_SOUND_SYSTEM',
          'POSIX',
          'SRTP_RELATIVE_PATH',
          'SSL_USE_OPENSSL',
          'USE_WEBRTC_DEV_BRANCH',
        ],
      },
      'export_dependent_settings': [
        '<(DEPTH)/native_client_sdk/native_client_sdk_untrusted.gyp:nacl_io_untrusted',
      ],
    },  # end of target 'libjingle_nacl'

    {
      'target_name': 'libjingle_p2p_constants_nacl',
      'type': 'none',
      'variables': {
        'nlib_target': 'libjingle_p2p_constants_nacl.a',
        'build_glibc': 0,
        'build_newlib': 1,
        'build_pnacl_newlib': 1,
      },
      'configurations': {
        'Debug': {
          'defines': [
            # TODO(sergeyu): Fix libjingle to use NDEBUG instead of
            # _DEBUG and remove this define. See below as well.
            '_DEBUG',
          ],
        }
      },
      'include_dirs': [
        './<(libjingle_source)',
      ],
      'sources': [
        '<(libjingle_source)/talk/p2p/base/constants.cc',
        '<(libjingle_source)/talk/p2p/base/constants.h',
      ],
    },  # end of target 'libjingle_p2p_constants_nacl'
  ],
}
