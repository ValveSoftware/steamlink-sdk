# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'web_dialogs',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../content/content.gyp:content_browser',
        '../../skia/skia.gyp:skia',
      ],
      'defines': [
        'WEB_DIALOGS_IMPLEMENTATION',
      ],
      'sources': [
        # All .cc, .h under web_dialogs, except unittests.
        'web_dialog_delegate.cc',
        'web_dialog_delegate.h',
        'web_dialog_ui.cc',
        'web_dialog_ui.h',
        'web_dialog_web_contents_delegate.cc',
        'web_dialog_web_contents_delegate.h',
        'web_dialogs_export.h',
      ],
    },
    {
      'target_name': 'web_dialogs_test_support',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../content/content.gyp:content_browser',
        '../../skia/skia.gyp:skia',
        'web_dialogs',
      ],
      'sources': [
        'test/test_web_contents_handler.cc',
        'test/test_web_contents_handler.h',
        'test/test_web_dialog_delegate.cc',
        'test/test_web_dialog_delegate.h',
      ],
    },
  ],
}
