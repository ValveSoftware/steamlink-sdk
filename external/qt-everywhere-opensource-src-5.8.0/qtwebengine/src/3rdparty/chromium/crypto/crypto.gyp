# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    'crypto.gypi',
  ],
  'targets': [
    {
      'target_name': 'crypto',
      'type': '<(component)',
      'product_name': 'crcrypto',  # Avoid colliding with OpenSSL's libcrypto
      'dependencies': [
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../third_party/boringssl/boringssl.gyp:boringssl',
      ],
      'defines': [
        'CRYPTO_IMPLEMENTATION',
      ],
      'conditions': [
        [ 'os_posix == 1 and OS != "mac" and OS != "ios" and OS != "android"', {
          'dependencies': [
            '../build/linux/system.gyp:nss',
          ],
          'export_dependent_settings': [
            '../build/linux/system.gyp:nss',
          ],
          'conditions': [
            [ 'chromeos==1', {
                'sources/': [ ['include', '_chromeos\\.cc$'] ]
              },
            ],
          ],
        }],
        [ 'OS != "mac" and OS != "ios"', {
          'sources!': [
            'apple_keychain.h',
            'mock_apple_keychain.cc',
            'mock_apple_keychain.h',
          ],
        }],
        [ 'os_bsd==1', {
          'link_settings': {
            'libraries': [
              '-L/usr/local/lib -lexecinfo',
              ],
            },
          },
        ],
        [ 'OS == "mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Security.framework',
            ],
          },
        }, {  # OS != "mac"
          'sources!': [
            'cssm_init.cc',
            'cssm_init.h',
            'mac_security_services_lock.cc',
            'mac_security_services_lock.h',
          ],
        }],
        [ 'OS != "win"', {
          'sources!': [
            'capi_util.h',
            'capi_util.cc',
          ],
        }],
        [ 'OS == "win"', {
          'msvs_disabled_warnings': [
            4267,  # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          ],
        }],
        [ 'use_nss_certs==0', {
            # Some files are built when NSS is used for the platform certificate library.
            'sources!': [
              'nss_key_util.cc',
              'nss_key_util.h',
              'nss_util.cc',
              'nss_util.h',
              'nss_util_internal.h',
            ],
        },],
      ],
      'sources': [
        '<@(crypto_sources)',
      ],
    },
    {
      'target_name': 'crypto_unittests',
      'type': 'executable',
      'sources': [
        'aead_unittest.cc',
        'curve25519_unittest.cc',
        'ec_private_key_unittest.cc',
        'ec_signature_creator_unittest.cc',
        'encryptor_unittest.cc',
        'hkdf_unittest.cc',
        'hmac_unittest.cc',
        'nss_key_util_unittest.cc',
        'nss_util_unittest.cc',
        'openssl_bio_string_unittest.cc',
        'p224_unittest.cc',
        'p224_spake_unittest.cc',
        'random_unittest.cc',
        'rsa_private_key_unittest.cc',
        'secure_hash_unittest.cc',
        'sha2_unittest.cc',
        'signature_creator_unittest.cc',
        'signature_verifier_unittest.cc',
        'symmetric_key_unittest.cc',
      ],
      'dependencies': [
        'crypto',
        'crypto_test_support',
        '../base/base.gyp:base',
        '../base/base.gyp:run_all_unittests',
        '../base/base.gyp:test_support_base',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/boringssl/boringssl.gyp:boringssl',
      ],
      'conditions': [
        [ 'use_nss_certs == 1', {
          'dependencies': [
            '../build/linux/system.gyp:nss',
          ],
        }],
        [ 'use_nss_certs == 0', {
          # Some files are built when NSS is used for the platform certificate library.
          'sources!': [
            'nss_key_util_unittest.cc',
            'nss_util_unittest.cc',
          ],
        }],
        [ 'OS == "win"', {
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS == "win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'crypto_nacl_win64',
          # We use the native APIs for the helper.
          'type': '<(component)',
          'dependencies': [
            '../base/base.gyp:base_win64',
            '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations_win64',
            '../third_party/boringssl/boringssl.gyp:boringssl_nacl_win64',
          ],
          'sources': [
            '<@(nacl_win64_sources)',
          ],
          'defines': [
           'CRYPTO_IMPLEMENTATION',
           '<@(nacl_win64_defines)',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
    ['use_nss_certs==1', {
      'targets': [
        {
          'target_name': 'crypto_test_support',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            'crypto',
          ],
          'sources': [
            'scoped_test_nss_db.cc',
            'scoped_test_nss_db.h',
            'scoped_test_nss_chromeos_user.cc',
            'scoped_test_nss_chromeos_user.h',
            'scoped_test_system_nss_key_slot.cc',
            'scoped_test_system_nss_key_slot.h',
          ],
          'conditions': [
            ['use_nss_certs==0', {
              'sources!': [
                'scoped_test_nss_db.cc',
                'scoped_test_nss_db.h',
              ],
            }],
            [ 'chromeos==0', {
              'sources!': [
                'scoped_test_nss_chromeos_user.cc',
                'scoped_test_nss_chromeos_user.h',
                'scoped_test_system_nss_key_slot.cc',
                'scoped_test_system_nss_key_slot.h',
              ],
            }],
          ],
        }
      ]}, {  # use_nss_certs==0
      'targets': [
        {
          'target_name': 'crypto_test_support',
          'type': 'none',
          'sources': [],
        }
    ]}],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'crypto_unittests_run',
          'type': 'none',
          'dependencies': [
            'crypto_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
                      ],
          'sources': [
            'crypto_unittests.isolate',
          ],
        },
      ],
    }],
  ],
}
