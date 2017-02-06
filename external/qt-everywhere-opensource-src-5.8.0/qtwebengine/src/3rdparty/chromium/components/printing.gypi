# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN: //components/printing/common:printing_common
      'target_name': 'printing_common',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/ipc/ipc.gyp:ipc',
        '<(DEPTH)/printing/printing.gyp:printing',
        '<(DEPTH)/third_party/WebKit/public/blink.gyp:blink',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
      ],
      'sources': [
        "printing/common/print_messages.cc",
        "printing/common/print_messages.h",
        "printing/common/printing_param_traits_macros.h",
      ],
    },{
       # GN: //components/printing/common:printing_renderer
      'target_name': 'printing_renderer',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/content/content.gyp:content_common',
        '<(DEPTH)/content/content.gyp:content_renderer',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/printing/printing.gyp:printing',
        '<(DEPTH)/third_party/WebKit/public/blink.gyp:blink',
        '<(DEPTH)/ui/base/ui_base.gyp:ui_base',
         # TODO - Reenable this with appropriate resources when adding print preview to Qt WebEngine
         # 'components_resources.gyp:components_resources',
        'printing_common',
      ],
      'sources': [
        'printing/renderer/print_web_view_helper.cc',
        'printing/renderer/print_web_view_helper.h',
        'printing/renderer/print_web_view_helper_android.cc',
        'printing/renderer/print_web_view_helper_linux.cc',
        'printing/renderer/print_web_view_helper_mac.mm',
        'printing/renderer/print_web_view_helper_pdf_win.cc',
      ],
      # TODO(dgn): C4267: http://crbug.com/167187 size_t -> int
      'msvs_disabled_warnings': [ 4267 ],
    },{
      # GN: //components/printing/browser:printing_browser
      'target_name': 'printing_browser',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/printing/printing.gyp:printing',
        '<(DEPTH)/skia/skia.gyp:skia',
      ],
      'sources': [
        'printing/browser/print_manager.cc',
        'printing/browser/print_manager.h',
        'printing/browser/print_manager_utils.cc',
        'printing/browser/print_manager_utils.h',
      ],
    },{
      # GN: //components/printing/test:printing_test_support
      'target_name': 'printing_test_support',
      'type': 'static_library',
      'dependencies': [
        '../content/content_shell_and_tests.gyp:test_support_content',
        '../skia/skia.gyp:skia',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'printing_renderer',
      ],
      'sources': [
        'printing/test/mock_printer.cc',
        'printing/test/mock_printer.h',
        'printing/test/print_mock_render_thread.cc',
        'printing/test/print_mock_render_thread.h',
        'printing/test/print_test_content_renderer_client.cc',
        'printing/test/print_test_content_renderer_client.h',
      ],
    },
  ],
}
