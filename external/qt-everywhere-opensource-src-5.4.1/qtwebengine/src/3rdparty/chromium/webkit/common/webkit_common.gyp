# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'enable_wexit_time_destructors': 1,
    'chromium_code': 1
  },
  'targets': [
    {
      # GN version: //webkit/common:common",
      'target_name': 'webkit_common',
      'type': '<(component)',
      'defines': [
        'WEBKIT_COMMON_IMPLEMENTATION',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/ui/base/ui_base.gyp:ui_base',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
        '<(DEPTH)/ui/resources/ui_resources.gyp:ui_resources',
        '<(DEPTH)/url/url.gyp:url_lib',
        '<(DEPTH)/webkit/webkit_resources.gyp:webkit_resources',
        '<(DEPTH)/third_party/WebKit/public/blink_headers.gyp:blink_headers',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/third_party/WebKit/public/blink_headers.gyp:blink_headers',
      ],

      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '<(SHARED_INTERMEDIATE_DIR)/ui',
        '<(SHARED_INTERMEDIATE_DIR)/webkit',
      ],

      'sources': [
        'data_element.cc',
        'data_element.h',
        'resource_type.cc',
        'resource_type.h',
        'webkit_common_export.h',
        'webpreferences.cc',
        'webpreferences.h',
      ],

      'conditions': [
        ['use_aura==1 and use_x11==1', {
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:xcursor',
          ],
        }],
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
            ],
          },
        }],
        ['OS=="win"', {
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4800, 4267 ],
        }],
      ],
    },
  ],
}
