# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //cc/blink
      'target_name': 'cc_blink',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/third_party/khronos/khronos.gyp:khronos_headers',
        '../../gpu/gpu.gyp:command_buffer_common',
        '../../base/base.gyp:base',
        '../../third_party/WebKit/public/blink.gyp:blink',
        '../../skia/skia.gyp:skia',
        '../cc.gyp:cc',
        '../../ui/gfx/gfx.gyp:gfx',
        '../../ui/gfx/gfx.gyp:gfx_geometry',
      ],
      'defines': [
        'CC_BLINK_IMPLEMENTATION',
      ],
      'export_dependent_settings': [
        '../../skia/skia.gyp:skia',
      ],
      # This sources list is duplicated in //cc/blink/BUILD.gn
      'sources': [
        'cc_blink_export.h',
        'context_provider_web_context.h',
        'scrollbar_impl.cc',
        'scrollbar_impl.h',
        'web_compositor_support_impl.cc',
        'web_compositor_support_impl.h',
        'web_content_layer_impl.cc',
        'web_content_layer_impl.h',
        'web_display_item_list_impl.cc',
        'web_display_item_list_impl.h',
        'web_external_bitmap_impl.cc',
        'web_external_bitmap_impl.h',
        'web_external_texture_layer_impl.cc',
        'web_external_texture_layer_impl.h',
        'web_image_layer_impl.cc',
        'web_image_layer_impl.h',
        'web_layer_impl.cc',
        'web_layer_impl.h',
        'web_layer_impl_fixed_bounds.cc',
        'web_layer_impl_fixed_bounds.h',
        'web_scrollbar_layer_impl.cc',
        'web_scrollbar_layer_impl.h',
      ],
    },
  ]
}
