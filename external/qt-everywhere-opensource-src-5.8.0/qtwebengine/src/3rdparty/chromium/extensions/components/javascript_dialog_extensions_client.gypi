# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'javascript_dialog_extensions_client',
      'type': 'static_library',
      'dependencies': [
        '../../components/components.gyp:app_modal',
        '../../skia/skia.gyp:skia',
        '../extensions.gyp:extensions_browser',
        '../extensions.gyp:extensions_common',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'javascript_dialog_extensions_client/javascript_dialog_extension_client_impl.cc',
        'javascript_dialog_extensions_client/javascript_dialog_extension_client_impl.h',
      ],
    },
  ],
}
