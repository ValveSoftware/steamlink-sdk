# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ozone_demo',
      'type': 'executable',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../skia/skia.gyp:skia',
        '../../../ui/events/ozone/events_ozone.gyp:events_ozone_layout',
        '../../../ui/gfx/gfx.gyp:gfx_geometry',
        '../../../ui/gl/gl.gyp:gl',
        '../../../ui/gl/init/gl_init.gyp:gl_init',
        '../../../ui/ozone/ozone.gyp:ozone',
        '../../../ui/ozone/ozone.gyp:ozone_base',
      ],
      'sources': [
        'gl_renderer.cc',
        'gl_renderer.h',
        'ozone_demo.cc',
        'renderer.h',
        'renderer_base.cc',
        'renderer_base.h',
        'software_renderer.cc',
        'software_renderer.h',
        'surfaceless_gl_renderer.cc',
        'surfaceless_gl_renderer.h',
      ],
    },
  ],
}
