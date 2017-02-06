# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'use_system_libpng%': 0,
  },
  'conditions' : [
    ['use_system_libpng == 0', {
  'targets': [
    {
      'target_name': 'libpng',
      'dependencies': [
        '../zlib/zlib.gyp:zlib',
      ],
      'variables': {
        # libpng checks that the width is not greater than PNG_SIZE_MAX.
        # On platforms where size_t is 64-bits, this comparison will always
        # be false.
        'clang_warning_flags': [ '-Wno-tautological-constant-out-of-range-compare' ],
      },
      'sources': [
        'png.c',
        'png.h',
        'pngconf.h',
        'pngerror.c',
        'pngget.c',
        'pnginfo.h',
        'pnglibconf.h',
        'pngmem.c',
        'pngpread.c',
        'pngprefix.h',
        'pngpriv.h',
        'pngread.c',
        'pngrio.c',
        'pngrtran.c',
        'pngrutil.c',
        'pngset.c',
        'pngstruct.h',
        'pngtrans.c',
        'pngwio.c',
        'pngwrite.c',
        'pngwtran.c',
        'pngwutil.c',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
      'export_dependent_settings': [
        '../zlib/zlib.gyp:zlib',
      ],
      'msvs_disabled_warnings': [
        4267, # TODO(jschuh): http://crbug.com/167187
        4146, # Unary minus applied to unsigned type.
      ],
      'conditions': [
        # Disable ARM optimizations on IOS.  Can't find a way to get gyp to even try
        # to compile the optimization files.  This works fine on GN.
        [ 'OS=="ios"', {
          'defines': [
            'PNG_ARM_NEON_OPT=0',
          ],
        }],

        # SSE optimizations
        [ 'target_arch=="ia32" or target_arch=="x64"', {
          'defines': [
            'PNG_INTEL_SSE_OPT=1',
          ],
          'sources': [
            'contrib/intel/intel_init.c',
            'contrib/intel/filter_sse2_intrinsics.c',
          ],
        }],

        # ARM optimizations
        [ '(target_arch=="arm" or target_arch=="arm64") and OS!="ios" and arm_neon==1', {
          'defines': [
            'PNG_ARM_NEON_OPT=2',
            'PNG_ARM_NEON_IMPLEMENTATION=1',
          ],
          'sources': [
            'arm/arm_init.c',
            'arm/filter_neon_intrinsics.c',
          ],
        }],
      
        ['OS!="win"', {'product_name': 'png'}],
        ['OS=="win"', {
          'type': '<(component)',
        }, {
          # Chromium libpng does not support building as a shared_library
          # on non-Windows platforms.
          'type': 'static_library',
        }],
        ['OS=="win" and component=="shared_library"', {
          'defines': [
            'PNG_BUILD_DLL',
            'PNG_NO_MODULEDEF',
          ],
          'direct_dependent_settings': {
            'defines': [
              'PNG_USE_DLL',
            ],
          },
        }],
        ['OS=="android"', {
          'toolsets': ['target', 'host'],
        }],
      ],
    },
  ]
  }, #  'use_system_libpng == 0'
  {
  'targets': [
    {
      'target_name': 'libpng',
      'type': 'none',
      'dependencies': [
        '../zlib/zlib.gyp:zlib',
      ],
      'direct_dependent_settings': {
        'cflags': [
          '<!@(<(pkg-config) --cflags libpng)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(<(pkg-config) --libs-only-L --libs-only-other libpng)',
        ],
        'libraries': [
          '<!@(<(pkg-config) --libs-only-l libpng)',
        ],
      },
      'variables': {
        'headers_root_path': '.',
        'header_filenames': [
          'png.h',
          'pngconf.h',
        ],
      },
      'includes': [
        '../../build/shim_headers.gypi',
      ],
    },
  ],
  }
  ]]
}
