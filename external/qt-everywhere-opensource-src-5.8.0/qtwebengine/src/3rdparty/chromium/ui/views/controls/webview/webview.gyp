# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ui/views/controls/webview
      'target_name': 'webview',
      'type': '<(component)',
      'dependencies': [
        '../../../../base/base.gyp:base',
        '../../../../base/base.gyp:base_i18n',
        '../../../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../../../content/content.gyp:content_browser',
        '../../../../skia/skia.gyp:skia',
        '../../../../url/url.gyp:url_lib',
        '../../../base/ui_base.gyp:ui_base',
        '../../../content_accelerators/ui_content_accelerators.gyp:ui_content_accelerators',
        '../../../events/events.gyp:events',
        '../../../gfx/gfx.gyp:gfx',
        '../../../gfx/gfx.gyp:gfx_geometry',
        '../../../web_dialogs/web_dialogs.gyp:web_dialogs',
        '../../views.gyp:views',
      ],
      'defines': [
        'WEBVIEW_IMPLEMENTATION',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'unhandled_keyboard_event_handler.cc',
        'unhandled_keyboard_event_handler.h',
        'unhandled_keyboard_event_handler_mac.mm',
        'unhandled_keyboard_event_handler_win.cc',
        'web_dialog_view.cc',
        'web_dialog_view.h',
        'webview.cc',
        'webview.h',
        'webview_export.h',
      ],
      'conditions': [
        ['OS=="linux" or OS=="android"', {
          'sources': [ 'unhandled_keyboard_event_handler_default.cc', ],
        }],
      ],
    },
  ],
}
