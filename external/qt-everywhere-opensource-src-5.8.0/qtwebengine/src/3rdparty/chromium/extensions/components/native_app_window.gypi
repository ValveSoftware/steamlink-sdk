# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'native_app_window',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../content/content.gyp:content_browser',
        '../../skia/skia.gyp:skia',
        '../../ui/views/views.gyp:views',
        '../../ui/views/controls/webview/webview.gyp:webview',
        '../extensions.gyp:extensions_browser',
        '../extensions.gyp:extensions_common',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'native_app_window/native_app_window_views.cc',
        'native_app_window/native_app_window_views.h',
      ],
    },
  ],
}
