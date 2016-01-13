{
  'variables': {
    'chromium_code': 1,
    'pdf_engine%': 0,  # 0 PDFium
  },
  'target_defaults': {
    'cflags': [
      '-fPIC',
    ],
  },
  'targets': [
    {
      'target_name': 'pdf',
      'type': 'loadable_module',
      'msvs_guid': '647863C0-C7A3-469A-B1ED-AD7283C34BED',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../ppapi/ppapi.gyp:ppapi_cpp',
        '../third_party/pdfium/pdfium.gyp:pdfium',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'Info.plist',
      },
      'mac_framework_dirs': [
        '$(SDKROOT)/System/Library/Frameworks/ApplicationServices.framework/Frameworks',
      ],
      'ldflags': [ '-L<(PRODUCT_DIR)',],
      'sources': [
        'button.h',
        'button.cc',
        'chunk_stream.h',
        'chunk_stream.cc',
        'control.h',
        'control.cc',
        'document_loader.h',
        'document_loader.cc',
        'draw_utils.cc',
        'draw_utils.h',
        'fading_control.cc',
        'fading_control.h',
        'fading_controls.cc',
        'fading_controls.h',
        'instance.cc',
        'instance.h',
        'number_image_generator.cc',
        'number_image_generator.h',
        'out_of_process_instance.cc',
        'out_of_process_instance.h',
        'page_indicator.cc',
        'page_indicator.h',
        'paint_aggregator.cc',
        'paint_aggregator.h',
        'paint_manager.cc',
        'paint_manager.h',
        'pdf.cc',
        'pdf.h',
        'pdf.rc',
        'progress_control.cc',
        'progress_control.h',
        'pdf_engine.h',
        'preview_mode_client.cc',
        'preview_mode_client.h',
        'resource.h',
        'resource_consts.h',
        'thumbnail_control.cc',
        'thumbnail_control.h',
        '../chrome/browser/chrome_page_zoom_constants.cc',
        '../content/common/page_zoom.cc',
      ],
      'conditions': [
        ['pdf_engine==0', {
          'sources': [
            'pdfium/pdfium_assert_matching_enums.cc',
            'pdfium/pdfium_engine.cc',
            'pdfium/pdfium_engine.h',
            'pdfium/pdfium_mem_buffer_file_read.cc',
            'pdfium/pdfium_mem_buffer_file_read.h',
            'pdfium/pdfium_mem_buffer_file_write.cc',
            'pdfium/pdfium_mem_buffer_file_write.h',
            'pdfium/pdfium_page.cc',
            'pdfium/pdfium_page.h',
            'pdfium/pdfium_range.cc',
            'pdfium/pdfium_range.h',
          ],
        }],
        ['OS!="win"', {
          'sources!': [
            'pdf.rc',
          ],
        }],
        ['OS=="mac"', {
          'mac_bundle': 1,
          'product_name': 'PDF',
          'product_extension': 'plugin',
          # Strip the shipping binary of symbols so "Foxit" doesn't appear in
          # the binary.  Symbols are stored in a separate .dSYM.
          'variables': {
            'mac_real_dsym': 1,
          },
          'sources+': [
            'Info.plist'
          ],
        }],
        ['OS=="win"', {
          'defines': [
            'COMPILE_CONTENT_STATICALLY',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        }],
        ['OS=="linux"', {
          'configurations': {
            'Release_Base': {
              #'cflags': [ '-fno-weak',],  # get rid of symbols that strip doesn't remove.
              # Don't do this for now since official builder will take care of it.  That
              # way symbols can still be uploaded to the crash server.
              #'ldflags': [ '-s',],  # strip local symbols from binary.
            },
          },
          # Use a custom version script to prevent leaking the vendor name in
          # visible symbols.
          'ldflags': [
            '-Wl,--version-script=<!(cd <(DEPTH) && pwd -P)/pdf/libpdf.map'
          ],
        }],
      ],
    },
  ],
  'conditions': [
    # CrOS has a separate step to do this.
    ['OS=="linux" and chromeos==0',
      { 'targets': [
        {
          'target_name': 'pdf_linux_symbols',
          'type': 'none',
          'conditions': [
            ['linux_dump_symbols==1', {
              'actions': [
                {
                  'action_name': 'dump_symbols',
                  'inputs': [
                    '<(DEPTH)/build/linux/dump_app_syms',
                    '<(PRODUCT_DIR)/dump_syms',
                    '<(PRODUCT_DIR)/libpdf.so',
                  ],
                  'outputs': [
                    '<(PRODUCT_DIR)/libpdf.so.breakpad.<(target_arch)',
                  ],
                  'action': ['<(DEPTH)/build/linux/dump_app_syms',
                             '<(PRODUCT_DIR)/dump_syms',
                             '<(linux_strip_binary)',
                             '<(PRODUCT_DIR)/libpdf.so',
                             '<@(_outputs)'],
                  'message': 'Dumping breakpad symbols to <(_outputs)',
                  'process_outputs_as_sources': 1,
                },
              ],
              'dependencies': [
                'pdf',
                '../breakpad/breakpad.gyp:dump_syms',
              ],
            }],
          ],
        },
      ],
    },],  # OS=="linux" and chromeos==0
    ['OS=="win" and fastbuild==0 and target_arch=="ia32" and syzyasan==1', {
      'variables': {
        'dest_dir': '<(PRODUCT_DIR)/syzygy',
      },
      'targets': [
        {
          'target_name': 'pdf_syzyasan',
          'type': 'none',
          'sources' : [],
          'dependencies': [
            'pdf',
          ],
          # Instrument PDFium with SyzyAsan.
          'actions': [
            {
              'action_name': 'Instrument PDFium with SyzyAsan',
              'inputs': [
                '<(PRODUCT_DIR)/pdf.dll',
              ],
              'outputs': [
                '<(dest_dir)/pdf.dll',
                '<(dest_dir)/pdf.dll.pdb',
              ],
              'action': [
                'python',
                '<(DEPTH)/chrome/tools/build/win/syzygy_instrument.py',
                '--mode', 'asan',
                '--input_executable', '<(PRODUCT_DIR)/pdf.dll',
                '--input_symbol', '<(PRODUCT_DIR)/pdf.dll.pdb',
                '--destination_dir', '<(dest_dir)',
              ],
            },
          ],
        },
      ],
    }],  # OS=="win" and fastbuild==0 and target_arch=="ia32" and syzyasan==1
  ],
}
