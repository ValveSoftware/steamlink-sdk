{
  'variables': {
    'chromium_code': 1,
    'pdf_enable_xfa%': 0,
    'pdf_engine%': 0,  # 0 PDFium
  },
  'targets': [
    {
      'target_name': 'pdf',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../gin/gin.gyp:gin',
        '../net/net.gyp:net',
        '../ppapi/ppapi.gyp:ppapi_cpp_objects',
        '../ppapi/ppapi.gyp:ppapi_internal_module',
      ],
      'ldflags': [ '-L<(PRODUCT_DIR)',],
      'sources': [
        'chunk_stream.h',
        'chunk_stream.cc',
        'document_loader.h',
        'document_loader.cc',
        'draw_utils.cc',
        'draw_utils.h',
        'out_of_process_instance.cc',
        'out_of_process_instance.h',
        'paint_aggregator.cc',
        'paint_aggregator.h',
        'paint_manager.cc',
        'paint_manager.h',
        'pdf.cc',
        'pdf.h',
        'pdf_engine.h',
        'preview_mode_client.cc',
        'preview_mode_client.h',
      ],
      'conditions': [
        ['pdf_engine==0', {
          'dependencies': [
            '../third_party/pdfium/pdfium.gyp:pdfium',
          ],
          'sources': [
            'pdfium/pdfium_api_string_buffer_adapter.cc',
            'pdfium/pdfium_api_string_buffer_adapter.h',
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
        ['pdf_enable_xfa==1', {
           'defines': [
             'PDF_ENABLE_XFA',
           ]
        }],
        ['OS=="win"', {
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        }],
      ],
    },
  ],
}
