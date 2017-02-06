# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'capture_sources': [
      'capture_export.h',
      'content/animated_content_sampler.cc',
      'content/animated_content_sampler.h',
      'content/capture_resolution_chooser.cc',
      'content/capture_resolution_chooser.h',
      'content/feedback_signal_accumulator.h',
      'content/screen_capture_device_core.cc',
      'content/screen_capture_device_core.h',
      'content/smooth_event_sampler.cc',
      'content/smooth_event_sampler.h',
      'content/thread_safe_capture_oracle.cc',
      'content/thread_safe_capture_oracle.h',
      'content/video_capture_oracle.cc',
      'content/video_capture_oracle.h',
      'device_monitor_mac.h',
      'device_monitor_mac.mm',
      'system_message_window_win.cc',
      'system_message_window_win.h',
      'video/android/video_capture_device_android.cc',
      'video/android/video_capture_device_android.h',
      'video/android/video_capture_device_factory_android.cc',
      'video/android/video_capture_device_factory_android.h',
      'video/fake_video_capture_device.cc',
      'video/fake_video_capture_device.h',
      'video/fake_video_capture_device_factory.cc',
      'video/fake_video_capture_device_factory.h',
      'video/file_video_capture_device.cc',
      'video/file_video_capture_device.h',
      'video/file_video_capture_device_factory.cc',
      'video/file_video_capture_device_factory.h',
      'video/linux/v4l2_capture_delegate.cc',
      'video/linux/v4l2_capture_delegate.h',
      'video/linux/video_capture_device_chromeos.cc',
      'video/linux/video_capture_device_chromeos.h',
      'video/linux/video_capture_device_factory_linux.cc',
      'video/linux/video_capture_device_factory_linux.h',
      'video/linux/video_capture_device_linux.cc',
      'video/linux/video_capture_device_linux.h',
      'video/mac/video_capture_device_avfoundation_mac.h',
      'video/mac/video_capture_device_avfoundation_mac.mm',
      'video/mac/video_capture_device_decklink_mac.h',
      'video/mac/video_capture_device_decklink_mac.mm',
      'video/mac/video_capture_device_factory_mac.h',
      'video/mac/video_capture_device_factory_mac.mm',
      'video/mac/video_capture_device_mac.h',
      'video/mac/video_capture_device_mac.mm',
      'video/scoped_result_callback.h',
      'video/video_capture_device.cc',
      'video/video_capture_device.h',
      'video/video_capture_device_factory.cc',
      'video/video_capture_device_factory.h',
      'video/video_capture_device_info.cc',
      'video/video_capture_device_info.h',
      'video/win/capability_list_win.cc',
      'video/win/capability_list_win.h',
      'video/win/filter_base_win.cc',
      'video/win/filter_base_win.h',
      'video/win/pin_base_win.cc',
      'video/win/pin_base_win.h',
      'video/win/sink_filter_observer_win.h',
      'video/win/sink_filter_win.cc',
      'video/win/sink_filter_win.h',
      'video/win/sink_input_pin_win.cc',
      'video/win/sink_input_pin_win.h',
      'video/win/video_capture_device_factory_win.cc',
      'video/win/video_capture_device_factory_win.h',
      'video/win/video_capture_device_mf_win.cc',
      'video/win/video_capture_device_mf_win.h',
      'video/win/video_capture_device_win.cc',
      'video/win/video_capture_device_win.h'
    ],

    'capture_unittests_sources': [
      'content/animated_content_sampler_unittest.cc',
      'content/capture_resolution_chooser_unittest.cc',
      'content/feedback_signal_accumulator_unittest.cc',
      'content/smooth_event_sampler_unittest.cc',
      'content/video_capture_oracle_unittest.cc',
      'system_message_window_win_unittest.cc',
      'video/fake_video_capture_device_unittest.cc',
      'video/mac/video_capture_device_factory_mac_unittest.mm',
      'video/video_capture_device_unittest.cc'
    ],

    # The following files lack appropriate platform suffixes.
    'conditions': [
      ['OS=="linux" and use_udev==1', {
        'capture_sources': [
          'device_monitor_udev.cc',
          'device_monitor_udev.h',
        ],
      }],
    ],
  },

  'targets': [
    {
      # GN version: //media/capture
      'target_name': 'capture',
      'type': '<(component)',
      'hard_dependency': 1,
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/media/media.gyp:media',
        '<(DEPTH)/media/media.gyp:shared_memory_support',  # For audio support.
        '<(DEPTH)/media/mojo/interfaces/mojo_bindings.gyp:image_capture_mojo_bindings',
        '<(DEPTH)/mojo/mojo_edk.gyp:mojo_system_impl',
        '<(DEPTH)/mojo/mojo_public.gyp:mojo_cpp_bindings',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
      ],
      'defines': [
        'CAPTURE_IMPLEMENTATION',
      ],
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'sources': [
        '<@(capture_sources)'
      ],
      'conditions': [
        ['OS=="android"', {
          'dependencies': [
            'capture_java',
            '<(DEPTH)/media/capture/video/android'
          ],
        }],
        ['OS=="mac"', {
          'dependencies': [
            '<(DEPTH)/third_party/decklink/decklink.gyp:decklink',
          ],
        }],
        ['chromeos==1', {
          'dependencies': [
            '<(DEPTH)/ui/display/display.gyp:display',
          ],
        }],
        ['OS=="linux" and use_udev==1', {
          'dependencies': [
            '<(DEPTH)/device/udev_linux/udev.gyp:udev_linux',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            '<(DEPTH)/media/media.gyp:mf_initializer',
          ],
          # TODO(jschuh): http://crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        }],
      ],
    },

    {
      # GN version: //media/capture:capture_unittests
      'target_name': 'capture_unittests',
      'type': '<(gtest_target_type)',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'dependencies': [
        'capture',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/media/media.gyp:media',
        '<(DEPTH)/media/mojo/interfaces/mojo_bindings.gyp:image_capture_mojo_bindings',
        '<(DEPTH)/mojo/mojo_edk.gyp:mojo_system_impl',
        '<(DEPTH)/mojo/mojo_public.gyp:mojo_cpp_bindings',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_test_support',
      ],
      'sources': [
        '<@(capture_unittests_sources)'
      ],
      'conditions': [
        ['OS=="android"', {
          'dependencies': [
            '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            '<(DEPTH)/media/media.gyp:mf_initializer',
          ],
          # TODO(jschuh): http://crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        }],
      ], # conditions
    },
  ],

  'conditions': [

    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          # There's no GN equivalent to this.
          'target_name': 'capture_unittests_run',
          'type': 'none',
          'dependencies': [
            'capture_unittests',
          ],
          'includes': [
            '../../build/isolate.gypi',
          ],
          'sources': [
            'capture_unittests.isolate',
          ]
        }
      ]
    }],

    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'capture_java',
          'type': 'none',
          'dependencies': [
            '/base/base.gyp:base',
            'media_android_captureapitype',
            'media_android_imageformat',
            'video_capture_android_jni_headers',
          ],
          'export_dependent_settings': [
            '../base/base.gyp:base',
          ],
          'variables': {
            'java_in_dir': 'video/android/java',
          },
          'includes': ['../../build/java.gypi'],
        },
        {
          'target_name': 'media_android_captureapitype',
          'type': 'none',
          'variables': {
            'source_file': 'video/video_capture_device.h',
          },
         'includes': [ '../../build/android/java_cpp_enum.gypi' ],
        },
        {
          'target_name': 'media_android_imageformat',
          'type': 'none',
          'variables': {
            'source_file': 'video/android/video_capture_device_android.h',
          },
          'includes': [ '../../build/android/java_cpp_enum.gypi' ],
        },
        {
          'target_name': 'video_capture_android_jni_headers',
          'type': 'none',
          'sources': [
            'video/android/java/src/org/chromium/media/VideoCapture.java',
            'video/android/java/src/org/chromium/media/VideoCaptureFactory.java',
          ],
          'variables': {
           'jni_gen_package': 'media',
          },
         'includes': ['../../build/jni_generator.gypi'],
        },

        {
          # There's no GN equivalent to this.
          'target_name': 'capture_unittests_apk',
          'type': 'none',
          'dependencies': [
            'capture_java',
            'capture_unittests',
          ],
          'variables': {
            'test_suite_name': 'capture_unittests',
          },
          'includes': ['../../build/apk_test.gypi'],
        },
      ],
      'conditions': [
        ['test_isolation_mode != "noop"', {
          'targets': [
            {
              'target_name': 'capture_unittests_apk_run',
              'type': 'none',
              'dependencies': [
                'capture_unittests_apk',
              ],
              'includes': [
                '../../build/isolate.gypi',
              ],
              'sources': [
                'capture_unittests_apk.isolate',
              ],
            },
          ],
        }],
      ],

    }],

  ],
}
