# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //chrome/browser/ui/views
      'target_name': 'browser_ui_views',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../ui/base/ui_base.gyp:ui_base',
        '../ui/events/events.gyp:events',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../url/url.gyp:url_lib',
      ],
      'conditions': [
        ['use_ash==1', {
          'dependencies': [
            '../ash/ash.gyp:ash',
          ],
        }],
      ],
      'defines': [
        'CHROME_VIEWS_IMPLEMENTATION',
      ],
      'sources': [
        'browser/ui/views/accelerator_table.cc',
        'browser/ui/views/accelerator_table.h',
        'browser/ui/views/chrome_views_export.h',
        'browser/ui/views/event_utils.cc',
        'browser/ui/views/event_utils.h',
        'browser/ui/views/tab_icon_view_model.h',
        'browser/ui/views/tabs/tab_renderer_data.cc',
        'browser/ui/views/tabs/tab_renderer_data.h',
        'browser/ui/views/tabs/tab_strip_observer.cc',
        'browser/ui/views/tabs/tab_strip_observer.h',
        'browser/ui/views/tabs/tab_strip_types.h',
      ],
    },
  ],
}
