# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    # In component mode (shared_lib), we build all of skia as a single DLL.
    # However, in the static mode, we need to build skia as multiple targets
    # in order to support the use case where a platform (e.g. Android) may
    # already have a copy of skia as a system library.
    ['component=="static_library"', {
      'targets': [
        {
          'target_name': 'skia_library',
          'type': 'static_library',
          # The optimize: 'max' scattered throughout are particularly
          # important when compiled by MSVC 2013, which seems
          # to mis-link-time-compile code that's built with
          # different optimization levels. http://crbug.com/543583
          'variables': {
            'optimize': 'max',
          },
          'includes': [
            'skia_common.gypi',
            'skia_library.gypi',
            '../build/android/increase_size_for_speed.gypi',
            # Disable LTO due to compiler error
            # in mems_in_disjoint_alias_sets_p, at alias.c:393
            # crbug.com/422255
            '../build/android/disable_gcc_lto.gypi',
          ],
        },
      ],
    }],
    ['component=="static_library"', {
      'targets': [
        {
          'target_name': 'skia',
          # The optimize: 'max' scattered throughout are particularly
          # important when compiled by MSVC 2013, which seems
          # to mis-link-time-compile code that's built with
          # different optimization levels. http://crbug.com/543583
          'variables': {
            'optimize': 'max',
          },
          'type': 'none',
          'dependencies': [
            'skia_library',
            'skia_chrome',
          ],
          'export_dependent_settings': [
            'skia_library',
            'skia_chrome',
          ],
          'direct_dependent_settings': {
            'conditions': [
              [ 'OS == "win"', {
                'defines': [
                  'GR_GL_FUNCTION_TYPE=__stdcall',
                ],
              }],
            ],
          },
        },
        {
          'target_name': 'skia_chrome',
          'type': 'static_library',
          'includes': [
            'skia_chrome.gypi',
            'skia_common.gypi',
            '../build/android/increase_size_for_speed.gypi',
          ],
        },
      ],
    },
    {  # component != static_library
      'targets': [
        {
          'target_name': 'skia',
          # The optimize: 'max' scattered throughout are particularly
          # important when compiled by MSVC 2013, which seems
          # to mis-link-time-compile code that's built with
          # different optimization levels. http://crbug.com/543583
          'variables': {
            'optimize': 'max',
          },
          'type': 'shared_library',
          'includes': [
            # Include skia_common.gypi first since it contains filename
            # exclusion rules. This allows the following includes to override
            # the exclusion rules.
            'skia_common.gypi',
            'skia_chrome.gypi',
            'skia_library.gypi',
            '../build/android/increase_size_for_speed.gypi',
          ],
          'defines': [
            'SKIA_DLL',
            'SKIA_IMPLEMENTATION=1',
            'GR_GL_IGNORE_ES3_MSAA=0',
          ],
          'direct_dependent_settings': {
            'conditions': [
              [ 'OS == "win"', {
                'defines': [
                  'GR_GL_FUNCTION_TYPE=__stdcall',
                ],
              }],
            ],
            'defines': [
              'SKIA_DLL',
              'GR_GL_IGNORE_ES3_MSAA=0',
            ],
          },
        },
        {
          'target_name': 'skia_library',
          'type': 'none',
        },
        {
          'target_name': 'skia_chrome',
          'type': 'none',
        },
      ],
    }],
  ],

  # targets that are not dependent upon the component type
  'targets': [
    {
      'target_name': 'image_operations_bench',
      # The optimize: 'max' scattered throughout are particularly
      # important when compiled by MSVC 2013, which seems
      # to mis-link-time-compile code that's built with
      # different optimization levels. http://crbug.com/543583
      'variables': {
        'optimize': 'max',
      },
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        'skia',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'ext/image_operations_bench.cc',
      ],
    },
    {
      'target_name': 'filter_fuzz_stub',
      'type': 'executable',
      # The optimize: 'max' scattered throughout are particularly
      # important when compiled by MSVC 2013, which seems
      # to mis-link-time-compile code that's built with
      # different optimization levels. http://crbug.com/543583
      'variables': {
        'optimize': 'max',
      },
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        'skia.gyp:skia',
      ],
      'sources': [
        'tools/filter_fuzz_stub/filter_fuzz_stub.cc',
      ],
      'includes': [
        '../build/android/increase_size_for_speed.gypi',
      ],
    },
    {
      'target_name': 'skia_interfaces_mojom',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'public/interfaces/bitmap.mojom',
        ],
        'mojom_typemaps': [
          'public/interfaces/skbitmap.typemap',
        ],
      },
      'includes': [ '../mojo/mojom_bindings_generator_explicit.gypi' ],
    },
    {
      'target_name': 'skia_mojo',
      'type': 'static_library',
      # The optimize: 'max' scattered throughout are particularly
      # important when compiled by MSVC 2013, which seems
      # to mis-link-time-compile code that's built with
      # different optimization levels. http://crbug.com/543583
      'variables': {
        'optimize': 'max',
      },
      'sources': [
        '../skia/public/interfaces/bitmap_skbitmap_struct_traits.cc',
      ],
      'dependencies': [
        'skia',
        'skia_interfaces_mojom',
        '../base/base.gyp:base',
      ],
      'export_dependent_settings': [
        'skia',
      ],
    },
  ],
}
