# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS=="win" or OS=="mac" or OS=="linux"', {
      'targets': [
        {
          # This is a target for the collection of cast development tools.  They
          # are not built/linked into the Chromium browser.
          'target_name': 'cast_testing_tools',
          'type': 'none',
          'dependencies': [
            'cast_receiver_app',
            'cast_sender_app',
            'cast_simulator',
            'udp_proxy',
          ],
        },
        {
          # GN version: //media/cast:cast_benchmarks
          'target_name': 'cast_benchmarks',
          'type': '<(gtest_target_type)',
          'include_dirs': [
            '<(DEPTH)/',
          ],
          'dependencies': [
            'cast_common',
            'cast_net',
            'cast_receiver',
            'cast_sender',
            'cast_test_utility',
            '<(DEPTH)/net/net.gyp:net',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'sources': [
            'test/cast_benchmarks.cc',
          ], # source
        },
        {
          # GN version: //media/cast:cast_receiver_app
          'target_name': 'cast_receiver_app',
          'type': 'executable',
          'include_dirs': [
            '<(DEPTH)/',
          ],
          'dependencies': [
            'cast_common',
            'cast_net',
            'cast_receiver',
            'cast_test_utility',
            '<(DEPTH)/third_party/libyuv/libyuv.gyp:libyuv',
          ],
          'sources': [
            '<(DEPTH)/media/cast/test/receiver.cc',
          ],
          'conditions': [
            ['OS == "linux" and use_x11==1', {
              'dependencies': [
                '<(DEPTH)/build/linux/system.gyp:x11',
                '<(DEPTH)/build/linux/system.gyp:xext',
              ],
              'sources': [
                '<(DEPTH)/media/cast/test/linux_output_window.cc',
                '<(DEPTH)/media/cast/test/linux_output_window.h',
              ],
            }],
          ],
        },
        {
          # GN version: //media/cast:cast_sender_app
          'target_name': 'cast_sender_app',
          'type': 'executable',
          'include_dirs': [
            '<(DEPTH)/',
          ],
          'dependencies': [
            'cast_common',
            'cast_net',
            'cast_sender',
            'cast_test_utility',
          ],
          'sources': [
            '<(DEPTH)/media/cast/test/sender.cc',
          ],
        },
        {
          # GN version: //media/cast:cast_simulator
          'target_name': 'cast_simulator',
          'type': 'executable',
          'include_dirs': [
            '<(DEPTH)/',
          ],
          'dependencies': [
            'cast_common',
            'cast_net',
            'cast_network_model_proto',
            'cast_sender',
            'cast_test_utility',
          ],
          'sources': [
            '<(DEPTH)/media/cast/test/simulator.cc',
          ],
        },
        {
          # GN version: //media/cast:network_model_proto
          'target_name': 'cast_network_model_proto',
          'type': 'static_library',
          'include_dirs': [
            '<(DEPTH)/',
          ],
          'sources': [
            'test/proto/network_simulation_model.proto',
          ],
          'variables': {
            'proto_in_dir': 'test/proto',
            'proto_out_dir': 'media/cast/test/proto',
          },
          'includes': ['../../build/protoc.gypi'],
        },
        {
          # GN version: //media/cast:generate_barcode_video
          'target_name': 'generate_barcode_video',
          'type': 'executable',
          'include_dirs': [
            '<(DEPTH)/',
          ],
          'dependencies': [
            'cast_test_utility',
            '<(DEPTH)/base/base.gyp:base',
            '<(DEPTH)/media/media.gyp:media',
          ],
          'sources': [
            'test/utility/generate_barcode_video.cc',
          ],
        },
        {
          # GN version: //media/cast:generate_timecode_audio
          'target_name': 'generate_timecode_audio',
          'type': 'executable',
          'include_dirs': [
            '<(DEPTH)/',
          ],
          'dependencies': [
            'cast_common',
            'cast_net',
            'cast_test_utility',
            '<(DEPTH)/base/base.gyp:base',
            '<(DEPTH)/media/media.gyp:media',
          ],
          'sources': [
            'test/utility/generate_timecode_audio.cc',
          ],
        },
        {
          # GN version: //media/cast:udp_proxy
          'target_name': 'udp_proxy',
          'type': 'executable',
          'include_dirs': [
            '<(DEPTH)/',
          ],
          'dependencies': [
            'cast_test_utility',
            '<(DEPTH)/base/base.gyp:base',
          ],
          'sources': [
            'test/utility/udp_proxy_main.cc',
          ],
        },
      ],
    }, {  # not (OS=="win" or OS=="mac" or OS=="linux")
      'targets': [
        {
          # The testing tools are only built for the desktop platforms.
          'target_name': 'cast_testing_tools',
          'type': 'none',
        },
      ],
    }],
    ['OS=="linux"', {
      'targets': [
        {
          # GN version: //media/cast:tap_proxy
          'target_name': 'tap_proxy',
          'type': 'executable',
          'include_dirs': [
            '<(DEPTH)/',
          ],
          'dependencies': [
            'cast_test_utility',
            '<(DEPTH)/base/base.gyp:base',
            '<(DEPTH)/media/media.gyp:media',
          ],
          'sources': [
            'test/utility/tap_proxy.cc',
          ],
        }
      ]
    }],
  ], # conditions
}
