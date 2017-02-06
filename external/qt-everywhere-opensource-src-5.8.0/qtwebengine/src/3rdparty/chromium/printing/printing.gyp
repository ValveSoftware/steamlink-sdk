# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'printing',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../skia/skia.gyp:skia',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../url/url.gyp:url_lib',
      ],
      'defines': [
        'PRINTING_IMPLEMENTATION',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'backend/print_backend.cc',
        'backend/print_backend.h',
        'backend/print_backend_consts.cc',
        'backend/print_backend_consts.h',
        'backend/print_backend_dummy.cc',
        'backend/print_backend_win.cc',
        'backend/printing_info_win.cc',
        'backend/printing_info_win.h',
        'emf_win.cc',
        'emf_win.h',
        'image.cc',
        'image.h',
        'image_android.cc',
        'image_linux.cc',
        'image_mac.cc',
        'image_win.cc',
        'metafile.cc',
        'metafile.h',
        'metafile_skia_wrapper.cc',
        'metafile_skia_wrapper.h',
        'page_number.cc',
        'page_number.h',
        'page_range.cc',
        'page_range.h',
        'page_setup.cc',
        'page_setup.h',
        'page_size_margins.h',
        'pdf_metafile_cg_mac.cc',
        'pdf_metafile_cg_mac.h',
        'pdf_metafile_skia.cc',
        'pdf_metafile_skia.h',
        'print_dialog_gtk_interface.h',
        'print_job_constants.cc',
        'print_job_constants.h',
        'print_settings.cc',
        'print_settings.h',
        'print_settings_conversion.cc',
        'print_settings_conversion.h',
        'print_settings_initializer_mac.cc',
        'print_settings_initializer_mac.h',
        'print_settings_initializer_win.cc',
        'print_settings_initializer_win.h',
        'printed_document.cc',
        'printed_document.h',
        'printed_document_linux.cc',
        'printed_document_mac.cc',
        'printed_document_win.cc',
        'printed_page.cc',
        'printed_page.h',
        'printed_pages_source.h',
        'printing_context.cc',
        'printing_context.h',
        'printing_context_system_dialog_win.cc',
        'printing_context_system_dialog_win.h',
        'printing_context_win.cc',
        'printing_context_win.h',
        'printing_utils.cc',
        'printing_utils.h',
        'units.cc',
        'units.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'conditions': [
        ['use_aura==1', {
          'dependencies': [
            '<(DEPTH)/ui/aura/aura.gyp:aura',
          ],
        }],
        # Mac-Aura does not support printing.
        ['OS=="mac" and use_aura==1',{
          'sources!': [
            'printed_document_mac.cc',
            'printing_context_mac.mm',
            'printing_context_mac.h',
          ],
        }],
        ['OS=="mac" and use_aura==0',{
          'sources': [
            'printing_context_mac.mm',
            'printing_context_mac.h',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            '<(DEPTH)/ui/aura/aura.gyp:aura',
          ],
          'defines': [
            # PRINT_BACKEND_AVAILABLE disables the default dummy implementation
            # of the print backend and enables a custom implementation instead.
            'PRINT_BACKEND_AVAILABLE',
          ],
          'sources': [
            'backend/win_helper.cc',
            'backend/win_helper.h',
          ],
        }],
        ['chromeos==1',{
          'sources': [
            'printing_context_no_system_dialog.cc',
            'printing_context_no_system_dialog.h',
          ],
        }],
        ['use_cups==1', {
          'dependencies': [
            'cups',
          ],
          'variables': {
            'cups_version': '<!(python cups_config_helper.py --api-version <(sysroot))',
          },
          'conditions': [
            ['cups_version in ["1.6", "1.7"]', {
              'cflags': [
                # CUPS 1.6 deprecated the PPD APIs, but we will stay with this
                # API for now as supported Linux and Mac OS'es are still using
                # older versions of CUPS. More info: crbug.com/226176
                '-Wno-deprecated-declarations',
                # CUPS 1.7 deprecates httpConnectEncrypt(), see the mac section
                # below.
              ],
            }],
            ['OS=="mac" and mac_sdk=="10.9"', {
              # The 10.9 SDK includes cups 1.7, which deprecates
              # httpConnectEncrypt() in favor of httpConnect2(). hhttpConnect2()
              # is new in 1.7, so it doesn't exist on OS X 10.6-10.8 and we
              # can't use it until 10.9 is our minimum system version.
              # (cups_version isn't reliable on OS X, so key the check off of
              # mac_sdk).
              'xcode_settings': {
                'WARNING_CFLAGS':  [
                  '-Wno-deprecated-declarations',
                ],
              },
            }],
          ],
          'defines': [
            # PRINT_BACKEND_AVAILABLE disables the default dummy implementation
            # of the print backend and enables a custom implementation instead.
            'PRINT_BACKEND_AVAILABLE',
          ],
          'sources': [
            'backend/cups_helper.cc',
            'backend/cups_helper.h',
            'backend/print_backend_cups.cc',
          ],
        }],
        ['OS=="linux" and chromeos==1', {
          'defines': [
            # PRINT_BACKEND_AVAILABLE disables the default dummy implementation
            # of the print backend and enables a custom implementation instead.
            'PRINT_BACKEND_AVAILABLE',
          ],
          'sources': [
            'backend/print_backend_chromeos.cc',
          ],
        }],
        ['OS=="linux" and chromeos==0', {
          'sources': [
            'printing_context_linux.cc',
            'printing_context_linux.h',
          ],
        }],
        ['OS=="android"', {
          'sources': [
            'printing_context_android.cc',
            'printing_context_android.h',
          ],
          'dependencies': [
            'printing_jni_headers',
          ],
        }, {
          'sources': [
            'pdf_transform.cc',
            'pdf_transform.h',
          ],
        }],
      ],
    },
    {
      'target_name': 'printing_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:run_all_unittests',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        '../ui/base/ui_base.gyp:ui_base',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../ui/gfx/gfx.gyp:gfx_test_support',
        'printing',
      ],
      'sources': [
        'emf_win_unittest.cc',
        'page_number_unittest.cc',
        'page_range_unittest.cc',
        'page_setup_unittest.cc',
        'pdf_metafile_cg_mac_unittest.cc',
        'printed_page_unittest.cc',
        'printing_context_win_unittest.cc',
        'printing_test.h',
        'printing_utils_unittest.cc',
        'units_unittest.cc',
      ],
      'conditions': [
        ['OS!="mac"', {'sources/': [['exclude', '_mac_unittest\\.(cc|mm?)$']]}],
        ['OS!="win"', {'sources/': [['exclude', '_win_unittest\\.cc$']]}],
        ['OS!="android"', {
          'sources': ['pdf_transform_unittest.cc']
        }],
        ['use_cups==1', {
          'defines': [
            'USE_CUPS',
          ],
          'sources': [
            'backend/cups_helper_unittest.cc',
          ],
        }],
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      # GN version: //printing:cups (config, not a target).
      'target_name': 'cups',
      'type': 'none',
      'conditions': [
        ['use_cups==1', {
          'direct_dependent_settings': {
            'defines': [
              'USE_CUPS',
            ],
            'conditions': [
              ['OS=="mac"', {
                'link_settings': {
                  'libraries': [
                    '$(SDKROOT)/usr/lib/libcups.dylib',
                  ]
                },
              }, {
                'link_settings': {
                  'libraries': [
                    '<!@(python cups_config_helper.py --libs <(sysroot))',
                  ],
                },
              }],
              ['os_bsd==1', {
                'cflags': [
                  '<!@(python cups_config_helper.py --cflags <(sysroot))',
                ],
              }],
            ],
          },
        }],
      ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          # GN: //printing:printing_jni_headers
          'target_name': 'printing_jni_headers',
          'type': 'none',
          'sources': [
            'android/java/src/org/chromium/printing/PrintingContext.java',
          ],
          'variables': {
            'jni_gen_package': 'printing',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          # GN: //printing:printing_java
          'target_name': 'printing_java',
          'type': 'none',
          'variables': {
            'java_in_dir': '../printing/android/java',
          },
          'dependencies': [
            '../base/base.gyp:base_java',
          ],
          'includes': [ '../build/java.gypi'  ],
        }
      ]
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'printing_unittests_run',
          'type': 'none',
          'dependencies': [
            'printing_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'printing_unittests.isolate',
          ],
        },
      ],
    }],
  ]
}
