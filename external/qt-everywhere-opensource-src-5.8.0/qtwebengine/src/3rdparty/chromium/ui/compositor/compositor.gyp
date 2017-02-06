# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'compositor',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/cc/cc.gyp:cc',
        '<(DEPTH)/cc/cc.gyp:cc_surfaces',
        '<(DEPTH)/gpu/gpu.gyp:command_buffer_common',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
        '<(DEPTH)/ui/gl/gl.gyp:gl',
      ],
      'defines': [
        'COMPOSITOR_IMPLEMENTATION',
      ],
      'sources': [
        'callback_layer_animation_observer.cc',
        'callback_layer_animation_observer.h',
        'canvas_painter.cc',
        'canvas_painter.h',
        'clip_recorder.cc',
        'clip_recorder.h',
        'closure_animation_observer.cc',
        'closure_animation_observer.h',
        'compositing_recorder.cc',
        'compositing_recorder.h',
        'compositor.cc',
        'compositor.h',
        'compositor_animation_observer.h',
        'compositor_export.h',
        'compositor_observer.h',
        'compositor_switches.cc',
        'compositor_switches.h',
        'compositor_vsync_manager.cc',
        'compositor_vsync_manager.h',
        'debug_utils.cc',
        'debug_utils.h',
        'dip_util.cc',
        'dip_util.h',
        'float_animation_curve_adapter.cc',
        'float_animation_curve_adapter.h',
        'layer.cc',
        'layer.h',
        'layer_animation_delegate.h',
        'layer_animation_element.cc',
        'layer_animation_element.h',
        'layer_animation_observer.cc',
        'layer_animation_observer.h',
        'layer_animation_sequence.cc',
        'layer_animation_sequence.h',
        'layer_animator.cc',
        'layer_animator.h',
        'layer_animator_collection.cc',
        'layer_animator_collection.h',
        'layer_delegate.h',
        'layer_owner.cc',
        'layer_owner.h',
        'layer_threaded_animation_delegate.h',
        'layer_tree_owner.cc',
        'layer_tree_owner.h',
        'layer_type.h',
        'paint_cache.cc',
        'paint_cache.h',
        'paint_context.cc',
        'paint_context.h',
        'paint_recorder.cc',
        'paint_recorder.h',
        'reflector.cc',
        'reflector.h',
        'scoped_animation_duration_scale_mode.cc',
        'scoped_animation_duration_scale_mode.h',
        'scoped_layer_animation_settings.cc',
        'scoped_layer_animation_settings.h',
        'transform_animation_curve_adapter.cc',
        'transform_animation_curve_adapter.h',
        'transform_recorder.cc',
        'transform_recorder.h',
      ],
      'conditions': [
        ['OS == "win" and use_aura == 1 and use_qt == 0', {
          # TODO(sky): before we make this real need to remove
          # IDR_BITMAP_BRUSH_IMAGE.
          'dependencies': [
            '<(DEPTH)/ui/resources/ui_resources.gyp:ui_resources',
            '<(angle_path)/src/angle.gyp:libEGL',
            '<(angle_path)/src/angle.gyp:libGLESv2',
          ],
        }],
        ['use_aura == 1 and qt_os=="win32" and qt_gl=="angle"', {
          'libraries': [
            '-l<(qt_egl_library)',
            '-l<(qt_glesv2_library)',
          ],
        }],
      ],
    },
    {
      'target_name': 'compositor_test_support',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/cc/cc.gyp:cc',
        '<(DEPTH)/cc/cc.gyp:cc_surfaces',
        '<(DEPTH)/cc/cc_tests.gyp:cc_test_support',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/WebKit/public/blink.gyp:blink_minimal',
        '<(DEPTH)/ui/base/ui_base.gyp:ui_base',
        '<(DEPTH)/ui/display/display.gyp:display',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
        '<(DEPTH)/ui/gl/gl.gyp:gl',
        'compositor',
      ],
      'sources': [
        'test/context_factories_for_test.cc',
        'test/context_factories_for_test.h',
        'test/draw_waiter_for_test.cc',
        'test/draw_waiter_for_test.h',
        'test/in_process_context_factory.cc',
        'test/in_process_context_factory.h',
        'test/in_process_context_provider.cc',
        'test/in_process_context_provider.h',
        'test/layer_animation_observer_test_api.cc',
        'test/layer_animation_observer_test_api.h',
        'test/layer_animator_test_controller.cc',
        'test/layer_animator_test_controller.h',
        'test/multi_layer_animator_test_controller.cc',
        'test/multi_layer_animator_test_controller.h',
        'test/multi_layer_animator_test_controller_delegate.h',
        'test/test_compositor_host.h',
        'test/test_compositor_host_android.cc',
        'test/test_compositor_host_mac.mm',
        'test/test_compositor_host_ozone.cc',
        'test/test_compositor_host_win.cc',
        'test/test_compositor_host_x11.cc',
        'test/test_layer_animation_delegate.cc',
        'test/test_layer_animation_delegate.h',
        'test/test_layer_animation_observer.cc',
        'test/test_layer_animation_observer.h',
        'test/test_layers.cc',
        'test/test_layers.h',
        'test/test_suite.cc',
        'test/test_suite.h',
        'test/test_utils.cc',
        'test/test_utils.h',
      ],
      'conditions': [
        ['use_x11==1', {
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:x11',
            '<(DEPTH)/ui/gfx/x/gfx_x11.gyp:gfx_x11',
          ]
        }]
      ]
    },
    {
      'target_name': 'compositor_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/cc/cc.gyp:cc',
        '<(DEPTH)/cc/cc.gyp:cc_surfaces',
        '<(DEPTH)/cc/cc_tests.gyp:cc_test_support',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/ui/base/ui_base.gyp:ui_base',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
        '<(DEPTH)/ui/gl/gl.gyp:gl',
        '<(DEPTH)/ui/resources/ui_resources.gyp:ui_resources',
        'compositor',
        'compositor_test_support',
      ],
      'sources': [
        'callback_layer_animation_observer_unittest.cc',
        'compositor_unittest.cc',
        'layer_animation_element_unittest.cc',
        'layer_animation_sequence_unittest.cc',
        'layer_animator_unittest.cc',
        'layer_owner_unittest.cc',
        'layer_unittest.cc',
        'run_all_unittests.cc',
        'transform_animation_curve_adapter_unittest.cc',
      ],
      'conditions': [
        # osmesa GL implementation is used on linux.
        ['OS=="linux"', {
          'dependencies': [
            '<(DEPTH)/third_party/mesa/mesa.gyp:osmesa',
          ],
        }],
        ['OS == "android"', {
          'dependencies': [
            '../../testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'compositor_unittests_run',
          'type': 'none',
          'dependencies': [
            'compositor_unittests',
          ],
          'includes': [ '../../build/isolate.gypi' ],
          'sources': [ 'compositor_unittests.isolate' ],
          'conditions': [
            ['use_x11 == 1', {
              'dependencies': [
                '../../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
              ],
            }],
          ],
        },
      ],
    }],
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'compositor_unittests_apk',
          'type': 'none',
          'dependencies': [
            'compositor_unittests',
          ],
          'variables': {
            'test_suite_name': 'compositor_unittests',
          },
          'includes': [
            '../../build/apk_test.gypi',
          ],
        },
    ]}]
  ],
}
