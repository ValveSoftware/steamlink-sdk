# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ui/views/controls/webview:test_support
      'target_name': 'webview_test_support',
      'type': 'static_library',
      'dependencies': [
        '../../../../base/base.gyp:base',
        '../../../../content/content.gyp:content',
        '../../../../content/content_shell_and_tests.gyp:test_support_content',
        '../../../../ipc/ipc.gyp:test_support_ipc',
        '../../../../skia/skia.gyp:skia',
        '../../../../testing/gtest.gyp:gtest',
        '../../../base/ui_base.gyp:ui_base',
        '../../../events/events.gyp:events',
        '../../../gfx/gfx.gyp:gfx',
        '../../../gfx/gfx.gyp:gfx_geometry',
        '../../views.gyp:views',
        '../../views.gyp:views_test_support',
        'webview.gyp:webview',
      ],
      'include_dirs': [
        '../../../..',
      ],
      'sources': [
        '../../test/webview_test_helper.cc',
        '../../test/webview_test_helper.h',
      ],
      'conditions': [
        ['use_aura==1', {
          'dependencies': [
            '../../../aura/aura.gyp:aura',
          ],
        }],
      ],
    },
  ],
}
