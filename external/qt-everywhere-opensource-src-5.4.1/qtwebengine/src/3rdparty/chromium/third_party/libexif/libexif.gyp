# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      ['OS == "linux" and chromeos==0', {
        'use_system_libexif%': 1,
      }, {  # OS != "linux" and chromeos==0
        'use_system_libexif%': 0,
      }],
    ],
  },
  'conditions': [
    ['use_system_libexif==0', {
      'targets': [
        {
          'target_name': 'libexif',
          'type': 'loadable_module',
          'sources': [
            'sources/libexif/exif-byte-order.c',
            'sources/libexif/exif-content.c',
            'sources/libexif/exif-data.c',
            'sources/libexif/exif-entry.c',
            'sources/libexif/exif-format.c',
            'sources/libexif/exif-ifd.c',
            'sources/libexif/exif-loader.c',
            'sources/libexif/exif-log.c',
            'sources/libexif/exif-mem.c',
            'sources/libexif/exif-mnote-data.c',
            'sources/libexif/exif-tag.c',
            'sources/libexif/exif-utils.c',
            'sources/libexif/canon/exif-mnote-data-canon.c',
            'sources/libexif/canon/mnote-canon-entry.c',
            'sources/libexif/canon/mnote-canon-tag.c',
            'sources/libexif/fuji/exif-mnote-data-fuji.c',
            'sources/libexif/fuji/mnote-fuji-entry.c',
            'sources/libexif/fuji/mnote-fuji-tag.c',
            'sources/libexif/olympus/exif-mnote-data-olympus.c',
            'sources/libexif/olympus/mnote-olympus-entry.c',
            'sources/libexif/olympus/mnote-olympus-tag.c',
            'sources/libexif/pentax/exif-mnote-data-pentax.c',
            'sources/libexif/pentax/mnote-pentax-entry.c',
            'sources/libexif/pentax/mnote-pentax-tag.c',
          ],
          'include_dirs': [
            'sources',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'sources',
            ],
          },
          'conditions': [
            ['clang==1', {
              'cflags': [
                '-Wno-enum-conversion', 
                '-Wno-switch',
                # libexif uses fabs(int) to cast to float.
                '-Wno-absolute-value',
              ],
              'xcode_settings': {
                'WARNING_CFLAGS': [
                  '-Wno-enum-conversion', 
                  '-Wno-switch',
                  '-Wno-format',
                  # libexif uses fabs(int) to cast to float.
                  '-Wno-absolute-value',
                ],
              },
            }],
            ['os_posix==1 and OS!="mac"', {
              'cflags!': ['-fvisibility=hidden'],
            }],
            ['OS=="mac"', {
              'conditions': [
               ['mac_breakpad==1', {
                  'variables': {
                    'mac_real_dsym': 1,
                  },
               }],
              ],
              'xcode_settings': {
                'GCC_SYMBOLS_PRIVATE_EXTERN': 'NO', # no -fvisibility=hidden
              },
            }],
            ['OS=="win"', {
              'product_name': 'libexif',
              'sources': [
                'libexif.def',
              ],
              'defines': [
                # This seems like a hack, but this is what WebKit Win does.
                'snprintf=_snprintf',
                'inline=__inline',
              ],
              'msvs_disabled_warnings': [
                4018, # size/unsigned mismatch
                4267, # size_t -> ExifLong truncation on amd64
              ],
            }],
          ],
        },
      ],
    }, { # 'use_system_libexif!=0
      'conditions': [
        ['sysroot!=""', {
          'variables': {
            'pkg-config': '../../build/linux/pkg-config-wrapper "<(sysroot)" "<(target_arch)" "<(system_libdir)"',
          },
        }, {
          'variables': {
            'pkg-config': 'pkg-config'
          },
        }],
      ],
      'targets': [
        {
          'target_name': 'libexif',
          'type': 'none',
          'direct_dependent_settings': {
            'cflags': [
                '<!@(<(pkg-config) --cflags libexif)',
            ],
            'defines': [
              'USE_SYSTEM_LIBEXIF',
            ],
          },
        }
      ],
    }],
  ]
}
