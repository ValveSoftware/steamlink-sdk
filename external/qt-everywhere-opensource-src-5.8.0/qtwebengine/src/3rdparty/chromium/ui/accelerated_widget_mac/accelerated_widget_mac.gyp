# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ui/accelerated_widget_mac
      'target_name': 'accelerated_widget_mac',
      'type': '<(component)',
      'sources': [
        'accelerated_widget_mac.h',
        'accelerated_widget_mac.mm',
        'accelerated_widget_mac_export.h',
        'ca_layer_tree_coordinator.h',
        'ca_layer_tree_coordinator.mm',
        'ca_renderer_layer_tree.h',
        'ca_renderer_layer_tree.mm',
        'display_link_mac.cc',
        'display_link_mac.h',
        'fullscreen_low_power_coordinator.h',
        'gl_renderer_layer_tree.h',
        'gl_renderer_layer_tree.mm',
        'io_surface_context.h',
        'io_surface_context.mm',
        'window_resize_helper_mac.cc',
        'window_resize_helper_mac.h',
      ],
      'defines': [
        'ACCELERATED_WIDGET_MAC_IMPLEMENTATION',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/khronos/khronos.gyp:khronos_headers',
        '<(DEPTH)/ui/base/ui_base.gyp:ui_base',
        '<(DEPTH)/ui/events/events.gyp:events_base',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
        '<(DEPTH)/ui/gl/gl.gyp:gl',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/AVFoundation.framework',
          '$(SDKROOT)/System/Library/Frameworks/CoreMedia.framework',
          '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
        ],
      },
    },
    {
      # GN version: //ui/accelerated_widget_mac:accelerated_widget_mac_unittests
      'target_name': 'accelerated_widget_mac_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_test_support',
        'accelerated_widget_mac',
      ],
      'sources': [
        'ca_layer_tree_unittest_mac.mm',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
        ],
      },
    },
  ],
}
