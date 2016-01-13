# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    [ 'os_posix == 1 and OS != "mac" and OS != "ios"', {
      'conditions': [
        ['sysroot!=""', {
          'variables': {
            'pkg-config': '../../../build/linux/pkg-config-wrapper "<(sysroot)" "<(target_arch)" "<(system_libdir)"',
          },
        }, {
          'variables': {
            'pkg-config': 'pkg-config'
          },
        }],
      ],
    }],
  ],

  'targets': [
    {
      'target_name': 'libssl',
      'type': '<(component)',
      'product_name': 'crssl',  # Don't conflict with OpenSSL's libssl
      'sources': [
        'ssl/authcert.c',
        'ssl/cmpcert.c',
        'ssl/derive.c',
        'ssl/dtlscon.c',
        'ssl/os2_err.c',
        'ssl/os2_err.h',
        'ssl/preenc.h',
        'ssl/prelib.c',
        'ssl/ssl.h',
        'ssl/ssl3con.c',
        'ssl/ssl3ecc.c',
        'ssl/ssl3ext.c',
        'ssl/ssl3gthr.c',
        'ssl/ssl3prot.h',
        'ssl/sslauth.c',
        'ssl/sslcon.c',
        'ssl/ssldef.c',
        'ssl/sslenum.c',
        'ssl/sslerr.c',
        'ssl/sslerr.h',
        'ssl/SSLerrs.h',
        'ssl/sslerrstrs.c',
        'ssl/sslgathr.c',
        'ssl/sslimpl.h',
        'ssl/sslinfo.c',
        'ssl/sslinit.c',
        'ssl/sslmutex.c',
        'ssl/sslmutex.h',
        'ssl/sslnonce.c',
        'ssl/sslplatf.c',
        'ssl/sslproto.h',
        'ssl/sslreveal.c',
        'ssl/sslsecur.c',
        'ssl/sslsnce.c',
        'ssl/sslsock.c',
        'ssl/sslt.h',
        'ssl/ssltrace.c',
        'ssl/sslver.c',
        'ssl/unix_err.c',
        'ssl/unix_err.h',
        'ssl/win32err.c',
        'ssl/win32err.h',
        'ssl/bodge/secitem_array.c',
      ],
      'sources!': [
        'ssl/os2_err.c',
        'ssl/os2_err.h',
      ],
      'defines': [
        'NO_PKCS11_BYPASS',
        'NSS_ENABLE_ECC',
        'USE_UTIL_DIRECTLY',
      ],
      'msvs_disabled_warnings': [4018, 4244, 4267],
      'conditions': [
        ['component == "shared_library"', {
          'conditions': [
            ['OS == "mac" or OS == "ios"', {
              'xcode_settings': {
                'GCC_SYMBOLS_PRIVATE_EXTERN': 'NO',
              },
            }],
            ['OS == "win"', {
              'sources': [
                'ssl/exports_win.def',
              ],
            }],
            ['os_posix == 1 and OS != "mac" and OS != "ios"', {
              'cflags!': ['-fvisibility=hidden'],
            }],
          ],
        }],
        [ 'clang == 1', {
          'cflags': [
            # See http://crbug.com/138571#c8. In short, sslsecur.c picks up the
            # system's cert.h because cert.h isn't in chromium's repo.
            '-Wno-incompatible-pointer-types',

            # There is a broken header guard in /usr/include/nss/secmod.h:
            # https://bugzilla.mozilla.org/show_bug.cgi?id=884072
            '-Wno-header-guard',
          ],
        }],
        [ 'OS == "linux"', {
          'link_settings': {
            'libraries': [
              '-ldl',
            ],
          },
        }],
        [ 'OS == "mac" or OS == "ios"', {
          'defines': [
            'XP_UNIX',
            'DARWIN',
            'XP_MACOSX',
          ],
        }],
        [ 'OS == "mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Security.framework',
            ],
          },
        }],
        [ 'OS == "win"', {
            'sources!': [
              'ssl/unix_err.c',
              'ssl/unix_err.h',
            ],
          },
          {  # else: OS != "win"
            'sources!': [
              'ssl/win32err.c',
              'ssl/win32err.h',
            ],
          },
        ],
        [ 'os_posix == 1 and OS != "mac" and OS != "ios"', {
          'include_dirs': [
            'ssl/bodge',
          ],
          'cflags': [
            '<!@(<(pkg-config) --cflags nss)',
          ],
          'ldflags': [
            '<!@(<(pkg-config) --libs-only-L --libs-only-other nss)',
          ],
          'libraries': [
            '<!@(<(pkg-config) --libs-only-l nss | sed -e "s/-lssl3//")',
          ],
        }],
        [ 'OS == "mac" or OS == "ios" or OS == "win"', {
          'sources/': [
            ['exclude', 'ssl/bodge/'],
          ],
          'conditions': [
            ['OS != "ios"', {
              'defines': [
                'NSS_PLATFORM_CLIENT_AUTH',
              ],
              'direct_dependent_settings': {
                'defines': [
                  'NSS_PLATFORM_CLIENT_AUTH',
                ],
              },
            }],
          ],
          'dependencies': [
            '../../../third_party/nss/nss.gyp:nspr',
            '../../../third_party/nss/nss.gyp:nss',
          ],
          'export_dependent_settings': [
            '../../../third_party/nss/nss.gyp:nspr',
            '../../../third_party/nss/nss.gyp:nss',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'ssl',
            ],
          },
        }],
      ],
      'configurations': {
        'Debug_Base': {
          'defines': [
            'DEBUG',
          ],
        },
      },
    },
  ],
}
