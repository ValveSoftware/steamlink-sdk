# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'v2',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:base_i18n',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../cc/cc.gyp:cc',
        '../../gpu/gpu.gyp:gpu',
        '../../skia/skia.gyp:skia',
        '../compositor/compositor.gyp:compositor',
        '../events/events.gyp:events',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
      ],
      'defines': [
        'V2_IMPLEMENTATION',
      ],
      'sources': [
        'public/event.h',
        'public/event_dispatcher.h',
        'public/event_handler.h',
        'public/event_target.h',
        'public/layout.h',
        'public/painter.h',
        'public/v2_export.h',
        'public/view.h',
        'public/view_observer.h',
        'public/window.h',
        'src/event.cc',
        'src/event_dispatcher.cc',
        'src/event_processor.cc',
        'src/layout.cc',
        'src/paint_processor.cc',
        'src/view.cc',
        'src/view_observer.cc',
        'src/view_private.cc',
        'src/view_private.h',
        'src/window.cc',
      ],
    },
    {
      'target_name': 'v2_unittests',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:test_support_base',
        '../../skia/skia.gyp:skia',
        '../../testing/gtest.gyp:gtest',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        'v2',
      ],
      'sources': [
        'src/v2_unittest.cc',
        'src/view_unittest.cc',
      ],
    },
  ],
}
