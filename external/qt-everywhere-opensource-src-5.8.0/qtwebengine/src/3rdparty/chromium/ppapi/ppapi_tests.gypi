# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //ppapi:ppapi_tests
      'target_name': 'ppapi_tests',
      'type': 'loadable_module',
      'include_dirs': [
        'lib/gl/include',
      ],
      'defines': [
        'GL_GLEXT_PROTOTYPES',
      ],
      'sources': [
        '<@(test_common_source_files)',
        '<@(test_trusted_source_files)',
      ],
      'dependencies': [
        'ppapi.gyp:ppapi_cpp',
        'ppapi_internal.gyp:ppapi_shared',
      ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [
            # Keep 'test_case.html.mock-http-headers' with 'test_case.html'.
            'tests/test_case.html',
            'tests/test_case.html.mock-http-headers',
            'tests/test_page.css',
            'tests/test_page.css.mock-http-headers',
            'tests/ppapi_nacl_tests_newlib.nmf',
          ],
        },
        {
          'destination': '<(PRODUCT_DIR)/test_url_loader_data',
          'files': [
            'tests/test_url_loader_data/hello.txt',
          ],
        },
      ],
      'run_as': {
        'action': [
          '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)chrome<(EXECUTABLE_SUFFIX)',
          '--enable-pepper-testing',
          '--register-pepper-plugins=$(TargetPath);application/x-ppapi-tests',
          'file://$(ProjectDir)/tests/test_case.html?testcase=',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'defines': [
            '_CRT_SECURE_NO_DEPRECATE',
            '_CRT_NONSTDC_NO_WARNINGS',
            '_CRT_NONSTDC_NO_DEPRECATE',
            '_SCL_SECURE_NO_DEPRECATE',
          ],
        }],
        ['OS=="mac"', {
          'mac_bundle': 1,
          'product_name': 'ppapi_tests',
          'product_extension': 'plugin',
        }],
      ],
# TODO(dmichael):  Figure out what is wrong with the script on Windows and add
#                  it as an automated action.
#      'actions': [
#        {
#          'action_name': 'generate_ppapi_include_tests',
#          'inputs': [],
#          'outputs': [
#            'tests/test_c_includes.c',
#            'tests/test_cc_includes.cc',
#          ],
#          'action': [
#            '<!@(python generate_ppapi_include_tests.py)',
#          ],
#        },
#      ],
    },
    {
      # GN version: //ppapi:power_saver_test_plugin
      'target_name': 'power_saver_test_plugin',
      'type': 'loadable_module',
      'sources': [
        'tests/power_saver_test_plugin.cc',
        'tests/test_utils.cc',
      ],
      'dependencies': [
        'ppapi.gyp:ppapi_cpp',
        'ppapi_internal.gyp:ppapi_shared',
      ],
      'conditions': [
        ['OS=="mac"', {
          'mac_bundle': 1,
          'product_name': 'power_saver_test_plugin',
          'product_extension': 'plugin',
        }],
      ],
    },
    {
      # GN version: //ppapi:blink_deprecated_test_plugin
      'target_name': 'blink_deprecated_test_plugin',
      'type': 'loadable_module',
      'sources': [
        'tests/blink_deprecated_test_plugin.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'ppapi.gyp:ppapi_cpp',
        'ppapi_internal.gyp:ppapi_shared',
      ],
      'conditions': [
        ['OS=="mac"', {
          'mac_bundle': 1,
          'product_name': 'blink_deprecated_test_plugin',
          'product_extension': 'plugin',
        }],
      ],
    },
    {
      # GN version: //ppapi:blink_test_plugin
      'target_name': 'blink_test_plugin',
      'type': 'loadable_module',
      'sources': [
        'tests/blink_test_plugin.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'ppapi.gyp:ppapi_cpp',
        'ppapi_internal.gyp:ppapi_shared',
      ],
      'conditions': [
        ['OS=="mac"', {
          'mac_bundle': 1,
          'product_name': 'blink_test_plugin',
          'product_extension': 'plugin',
        }],
      ],
    },
    {
      # GN version: //ppapi/proxy:test_support
      #             //ppapi/shared_impl:test_support
      'target_name': 'ppapi_unittest_shared',
      'type': 'static_library',
      'dependencies': [
        'ppapi_proxy',
        'ppapi_shared',
        '../base/base.gyp:test_support_base',
        '../ipc/ipc.gyp:ipc',
        '../ipc/ipc.gyp:test_support_ipc',
        '../mojo/mojo_edk.gyp:mojo_system_impl',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'proxy/ppapi_proxy_test.cc',
        'proxy/ppapi_proxy_test.h',
        'proxy/resource_message_test_sink.cc',
        'proxy/resource_message_test_sink.h',
        'shared_impl/test_globals.cc',
        'shared_impl/test_globals.h',
        'shared_impl/test_utils.cc',
        'shared_impl/test_utils.h',
      ],
    },

    {
      # GN version: //ppapi:ppapi_perftests
      'target_name': 'ppapi_perftests',
      'type': 'executable',
      'variables': {
        'chromium_code': 1,
      },
      'dependencies': [
        'ppapi_proxy',
        'ppapi_shared',
        'ppapi_unittest_shared',
        '../base/base.gyp:test_support_base',
        '../mojo/mojo_edk.gyp:mojo_system_impl',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'proxy/ppapi_perftests.cc',
        'proxy/ppp_messaging_proxy_perftest.cc',
      ],
    },
    {
      # GN version: //ppapi:ppapi_unittests
      'target_name': 'ppapi_unittests',
      'type': 'executable',
      'variables': {
        'chromium_code': 1,
      },
      'dependencies': [
        'ppapi_host',
        'ppapi_proxy',
        'ppapi_shared',
        'ppapi_unittest_shared',
        '../base/base.gyp:test_support_base',
        '../gpu/gpu.gyp:gpu_ipc_common',
        '../ipc/ipc.gyp:ipc',
        '../ipc/ipc.gyp:ipc_run_all_unittests',
        '../ipc/ipc.gyp:test_support_ipc',
        '../media/media.gyp:shared_memory_support',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../ui/surface/surface.gyp:surface',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'host/resource_message_filter_unittest.cc',
        'proxy/device_enumeration_resource_helper_unittest.cc',
        'proxy/file_chooser_resource_unittest.cc',
        'proxy/file_system_resource_unittest.cc',
        'proxy/flash_resource_unittest.cc',
        'proxy/interface_list_unittest.cc',
        'proxy/mock_resource.cc',
        'proxy/mock_resource.h',
        'proxy/nacl_message_scanner_unittest.cc',
        'proxy/pdf_resource_unittest.cc',
        'proxy/plugin_dispatcher_unittest.cc',
        'proxy/plugin_resource_tracker_unittest.cc',
        'proxy/plugin_var_tracker_unittest.cc',
        'proxy/ppb_var_unittest.cc',
        'proxy/ppp_instance_private_proxy_unittest.cc',
        'proxy/ppp_instance_proxy_unittest.cc',
        'proxy/ppp_messaging_proxy_unittest.cc',
        'proxy/printing_resource_unittest.cc',
        'proxy/raw_var_data_unittest.cc',
        'proxy/serialized_var_unittest.cc',
        'proxy/tracked_callback_unittest.cc',
        'proxy/video_decoder_resource_unittest.cc',
        'proxy/video_encoder_resource_unittest.cc',
        'proxy/websocket_resource_unittest.cc',
        'shared_impl/media_stream_audio_track_shared_unittest.cc',
        'shared_impl/media_stream_buffer_manager_unittest.cc',
        'shared_impl/media_stream_video_track_shared_unittest.cc',
        'shared_impl/proxy_lock_unittest.cc',
        'shared_impl/resource_tracker_unittest.cc',
        'shared_impl/thread_aware_callback_unittest.cc',
        'shared_impl/time_conversion_unittest.cc',
        'shared_impl/var_tracker_unittest.cc',
      ],
    },
    {
      'target_name': 'ppapi_example_skeleton',
      'suppress_wildcard': 1,
      'type': 'none',
      'direct_dependent_settings': {
        'product_name': '>(_target_name)',
        'conditions': [
          ['os_posix==1 and OS!="mac"', {
            'cflags': ['-fvisibility=hidden'],
            'type': 'shared_library',
          }],
          ['OS=="win"', {
            'type': 'shared_library',
          }],
          ['OS=="mac"', {
            'type': 'loadable_module',
            'mac_bundle': 1,
            'product_extension': 'plugin',
            'xcode_settings': {
              'OTHER_LDFLAGS': [
                # Not to strip important symbols by -Wl,-dead_strip.
                '-Wl,-exported_symbol,_PPP_GetInterface',
                '-Wl,-exported_symbol,_PPP_InitializeModule',
                '-Wl,-exported_symbol,_PPP_ShutdownModule'
              ]},
          }],
        ],
      },
    },
    {
      # GN version: //ppapi/examples/mouse_cursor
      'target_name': 'ppapi_example_mouse_cursor',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/mouse_cursor/mouse_cursor.cc',
      ],
    },
    {
      # GN version: //ppapi/examples/mouse_lock
      'target_name': 'ppapi_example_mouse_lock',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/mouse_lock/mouse_lock.cc',
      ],
    },

    {
      # GN version: //ppapi/examples/gamepad
      'target_name': 'ppapi_example_gamepad',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/gamepad/gamepad.cc',
      ],
    },

    {
      # GN version: //ppapi/examples/stub:c_stub
      'target_name': 'ppapi_example_c_stub',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_c',
      ],
      'sources': [
        'examples/stub/stub.c',
      ],
    },
    {
      # GN version: //ppapi/examples/stub:cc_stub
      'target_name': 'ppapi_example_cc_stub',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/stub/stub.cc',
      ],
    },
    {
      # GN version: //ppapi/examples/crxfs
      'target_name': 'ppapi_example_crxfs',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/crxfs/crxfs.cc',
      ],
    },
    {
      # GN version: //ppapi/examples/audio
      'target_name': 'ppapi_example_audio',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/audio/audio.cc',
      ],
    },
    {
      'target_name': 'ppapi_example_audio_encode',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/audio_encode/audio_encode.cc',
      ],
    },
    {
      # GN version: //ppapi/examples/audio_input
      'target_name': 'ppapi_example_audio_input',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/audio_input/audio_input.cc',
      ],
    },
    {
      # GN version: //ppapi/examples/file_chooser
      'target_name': 'ppapi_example_file_chooser',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/file_chooser/file_chooser.cc',
      ],
    },
    {
      # GN version: //ppapi/examples/2d:graphics_2d
      'target_name': 'ppapi_example_graphics_2d',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_c',
      ],
      'sources': [
        'examples/2d/graphics_2d_example.c',
      ],
    },
    {
      # GN version: //ppapi/examples/ime
      'target_name': 'ppapi_example_ime',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/ime/ime.cc',
      ],
    },
    {
      # GN version: //ppapi/examples/2d:paint_manager
      'target_name': 'ppapi_example_paint_manager',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/2d/paint_manager_example.cc',
      ],
    },
    {
      # GN version: //ppapi/examples/input
      'target_name': 'ppapi_example_input',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/input/pointer_event_input.cc',
      ],
    },
    {
      # GN version: //ppapi/examples/scripting
      'target_name': 'ppapi_example_post_message',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/scripting/post_message.cc',
      ],
    },
    {
      # GN version: //ppapi/examples/scaling
      'target_name': 'ppapi_example_scaling',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/scaling/scaling.cc',
      ],
    },
    {
      # GN version: //ppapi/examples/2d:scroll
      'target_name': 'ppapi_example_scroll',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/2d/scroll.cc',
      ],
    },
    {
      # GN version: //ppapi/examples/font
      'target_name': 'ppapi_example_simple_font',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/font/simple_font.cc',
      ],
    },
    {
      # GN version: //ppapi/examples/threading
      'target_name': 'ppapi_example_threading',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/threading/threading.cc',
      ],
    },
    {
      # GN version: //ppapi/examples/url_loader:streaming
      'target_name': 'ppapi_example_url_loader',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/url_loader/streaming.cc',
      ],
    },
    {
      # GN version: //ppapi/examples/url_loader:file
      'target_name': 'ppapi_example_url_loader_file',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/url_loader/stream_to_file.cc',
      ],
    },
    {
      # GN version: //ppapi/examples/gles2
      'target_name': 'ppapi_example_gles2',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
        'ppapi.gyp:ppapi_gles2',
      ],
      'include_dirs': [
        'lib/gl/include',
      ],
      'sources': [
        'examples/gles2/gles2.cc',
      ],
    },
    {
      # GN version: //ppapi/examples/video_decode:stable
      'target_name': 'ppapi_example_video_decode',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
        'ppapi.gyp:ppapi_gles2',
      ],
      'include_dirs': [
        'lib/gl/include',
      ],
      'sources': [
        'examples/video_decode/testdata.h',
        'examples/video_decode/video_decode.cc',
      ],
    },
    {
      # GN version: //ppapi/examples/video_decode:dev
      'target_name': 'ppapi_example_video_decode_dev',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
        'ppapi.gyp:ppapi_gles2',
      ],
      'include_dirs': [
        'lib/gl/include',
      ],
      'sources': [
        'examples/video_decode/testdata.h',
        'examples/video_decode/video_decode_dev.cc',
      ],
    },
    {
      'target_name': 'ppapi_example_video_encode',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/video_encode/video_encode.cc',
      ],
    },
    {
      # GN version: //ppapi/example/video_capture
      'target_name': 'ppapi_example_vc',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
        'ppapi.gyp:ppapi_gles2',
      ],
      'include_dirs': [
        'lib/gl/include',
      ],
      'sources': [
        'examples/video_capture/video_capture.cc',
      ],
    },
    {
      # GN version: //ppapi/examples/video_effects
      'target_name': 'ppapi_example_video_effects',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/video_effects/video_effects.cc',
      ],
    },
    {
      # GN version: //ppapi/examples/enumerate_devices
      'target_name': 'ppapi_example_enumerate_devices',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/enumerate_devices/enumerate_devices.cc',
      ],
    },
    {
      # GN version: //ppapi/examples/flash_topmost
      'target_name': 'ppapi_example_flash_topmost',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/flash_topmost/flash_topmost.cc',
      ],
    },
    {
      # GN version: //ppapi/examples/printing
      'target_name': 'ppapi_example_printing',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/printing/printing.cc',
      ],
    },
    {
      # GN version: //ppapi/examples/media_stream_audio
      'target_name': 'ppapi_example_media_stream_audio',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/media_stream_audio/media_stream_audio.cc',
      ],
    },
    {
      # GN version: //ppapi/examples/media_stream_video
      'target_name': 'ppapi_example_media_stream_video',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
        'ppapi.gyp:ppapi_gles2',
      ],
      'include_dirs': [
        'lib/gl/include',
      ],
      'sources': [
        'examples/media_stream_video/media_stream_video.cc',
      ],
    },
    {
      # GN version: //ppapi/examples/gles2_spinning_cube
      'target_name': 'ppapi_example_gles2_spinning_cube',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
        'ppapi.gyp:ppapi_gles2',
      ],
      'include_dirs': [
        'lib/gl/include',
      ],
      'sources': [
        'examples/gles2_spinning_cube/gles2_spinning_cube.cc',
        'examples/gles2_spinning_cube/spinning_cube.cc',
        'examples/gles2_spinning_cube/spinning_cube.h',
      ],
    },
    {
      # GN version: //ppapi/examples/compositor
      'target_name': 'ppapi_example_compositor',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
        'ppapi.gyp:ppapi_gles2',
      ],
      'include_dirs': [
        'lib/gl/include',
      ],
      'sources': [
        'examples/compositor/compositor.cc',
        'examples/compositor/spinning_cube.cc',
        'examples/compositor/spinning_cube.h',
      ],
    },
    # Adding a new PPAPI example? Don't forget to update the GN build.
    # See //ppapi/examples/BUILD.gn
  ],
  'conditions': [
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'ppapi_unittests_run',
          'type': 'none',
          'dependencies': [
            'ppapi_unittests',
          ],
          'includes': [ '../build/isolate.gypi' ],
          'sources': [ 'ppapi_unittests.isolate' ],
        },
      ],
    }],
  ],
}
