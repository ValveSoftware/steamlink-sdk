# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [ 'cast_testing_tools.gypi' ],
  'targets': [
    {
      # GN version: //media/cast:test_support
      'target_name': 'cast_test_utility',
      'type': 'static_library',
      'include_dirs': [
         '<(DEPTH)/',
      ],
      'dependencies': [
        'cast_net',
        'cast_receiver',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/media/media.gyp:media_features',
        '<(DEPTH)/media/media.gyp:media_test_support',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/libyuv/libyuv.gyp:libyuv',
        '<(DEPTH)/third_party/mt19937ar/mt19937ar.gyp:mt19937ar',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_test_support',
      ],
      'sources': [
        'test/fake_receiver_time_offset_estimator.cc',
        'test/fake_receiver_time_offset_estimator.h',
        'test/loopback_transport.cc',
        'test/loopback_transport.h',
        'test/skewed_single_thread_task_runner.cc',
        'test/skewed_single_thread_task_runner.h',
        'test/skewed_tick_clock.cc',
        'test/skewed_tick_clock.h',
        'test/utility/audio_utility.cc',
        'test/utility/audio_utility.h',
        'test/utility/barcode.cc',
        'test/utility/barcode.h',
        'test/utility/default_config.cc',
        'test/utility/default_config.h',
        'test/utility/in_process_receiver.cc',
        'test/utility/in_process_receiver.h',
        'test/utility/input_builder.cc',
        'test/utility/input_builder.h',
        'test/utility/net_utility.cc',
        'test/utility/net_utility.h',
        'test/utility/standalone_cast_environment.cc',
        'test/utility/standalone_cast_environment.h',
        'test/utility/test_util.cc',
        'test/utility/test_util.h',
        'test/utility/udp_proxy.cc',
        'test/utility/udp_proxy.h',
        'test/utility/video_utility.cc',
        'test/utility/video_utility.h',
      ], # sources
      'conditions': [
        # FFMPEG software video decoders are not available on Android and/or
        # Chromecast content_shell builds.
        #
        # TODO(miu): There *are* hardware decoder APIs available via FFMPEG, and
        # we should use those for the Android receiver.  See media.gyp for usage
        # details.  http://crbug.com/558714
        ['OS!="android" and chromecast==0', {
          'dependencies': [
            '<(DEPTH)/third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
          ],
          'sources': [
            'test/fake_media_source.cc',
            'test/fake_media_source.h',
          ],
        }],
      ], # conditions
    },
    {
      # GN version: //media/cast:cast_unittests
      'target_name': 'cast_unittests',
      'type': '<(gtest_target_type)',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'dependencies': [
        'cast_common',
        'cast_sender',
        'cast_test_utility',
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'sources': [
        'common/expanded_value_base_unittest.cc',
        'common/rtp_time_unittest.cc',
        'logging/encoding_event_subscriber_unittest.cc',
        'logging/receiver_time_offset_estimator_impl_unittest.cc',
        'logging/serialize_deserialize_test.cc',
        'logging/simple_event_subscriber_unittest.cc',
        'logging/stats_event_subscriber_unittest.cc',
        'net/cast_transport_impl_unittest.cc',
        'net/mock_cast_transport.cc',
        'net/mock_cast_transport.h',
        'net/pacing/mock_paced_packet_sender.cc',
        'net/pacing/mock_paced_packet_sender.h',
        'net/pacing/paced_sender_unittest.cc',
        'net/rtcp/receiver_rtcp_event_subscriber_unittest.cc',
        'net/rtcp/rtcp_builder_unittest.cc',
        'net/rtcp/rtcp_unittest.cc',
        'net/rtcp/rtcp_utility_unittest.cc',
        # TODO(miu): The following two are test utility modules.  Rename/move
        # the files.
        'net/rtcp/test_rtcp_packet_builder.cc',
        'net/rtcp/test_rtcp_packet_builder.h',
        'net/rtp/cast_message_builder_unittest.cc',
        'net/rtp/frame_buffer_unittest.cc',
        'net/rtp/framer_unittest.cc',
        'net/rtp/mock_rtp_payload_feedback.cc',
        'net/rtp/mock_rtp_payload_feedback.h',
        'net/rtp/packet_storage_unittest.cc',
        'net/rtp/receiver_stats_unittest.cc',
        'net/rtp/rtp_packet_builder.cc',
        'net/rtp/rtp_packetizer_unittest.cc',
        'net/rtp/rtp_parser_unittest.cc',
        'net/udp_transport_unittest.cc',
        'receiver/audio_decoder_unittest.cc',
        'receiver/frame_receiver_unittest.cc',
        'receiver/video_decoder_unittest.cc',
        'sender/audio_encoder_unittest.cc',
        'sender/audio_sender_unittest.cc',
        'sender/congestion_control_unittest.cc',
        'sender/external_video_encoder_unittest.cc',
        'sender/fake_video_encode_accelerator_factory.cc',
        'sender/fake_video_encode_accelerator_factory.h',
        'sender/video_encoder_unittest.cc',
        'sender/video_sender_unittest.cc',
        'sender/vp8_quantizer_parser_unittest.cc',
        'test/end2end_unittest.cc',
        'test/utility/audio_utility_unittest.cc',
        'test/utility/barcode_unittest.cc',
      ], # sources
      'conditions': [
        ['OS=="android"', {
          'dependencies': [
            '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
      ], # conditions
    },
  ], # targets
  'conditions': [
    ['OS=="ios" or OS=="mac"', {
      'targets': [
        {
          # GN version: //media/cast:cast_h264_vt_encoder_unittests
          # TODO(miu): This can be rolled into cast_unittests now that FFMPEG
          # dependency issues are resolved for iOS; but there are bot/isolates
          # to update too.
          'target_name': 'cast_h264_vt_encoder_unittests',
          'type': '<(gtest_target_type)',
          'include_dirs': [
            '<(DEPTH)/',
          ],
          'dependencies': [
            'cast_common',
            'cast_sender',
            'cast_test_utility',
            '<(DEPTH)/testing/gmock.gyp:gmock',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(DEPTH)/third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
          ],
          'sources': [
            'sender/h264_vt_encoder_unittest.cc',
          ],
      }], # targets
    }], # OS=="ios" or OS=="mac"
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'cast_unittests_apk',
          'type': 'none',
          'dependencies': [
            'cast_unittests',
          ],
          'variables': {
            'test_suite_name': 'cast_unittests',
            'isolate_file': 'cast_unittests.isolate',
          },
          'includes': ['../../build/apk_test.gypi'],
        },
      ],
    }], # OS=="android"
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'cast_unittests_run',
          'type': 'none',
          'dependencies': [
            'cast_unittests',
          ],
          'includes': [
            '../../build/isolate.gypi',
          ],
          'sources': [
            'cast_unittests.isolate',
          ],
          'conditions': [
            ['use_x11==1',
              {
                'dependencies': [
                  '<(DEPTH)/tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
                ],
              }
            ],
          ],
        },
      ],
    }], # test_isolation_mode != "noop"
  ], # conditions
}
