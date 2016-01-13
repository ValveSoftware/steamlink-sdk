# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    # Remoting unit tests
    {
      'target_name': 'remoting_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../ipc/ipc.gyp:ipc',
        '../net/net.gyp:net_test_support',
        '../ppapi/ppapi.gyp:ppapi_cpp',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/libyuv/libyuv.gyp:libyuv',
        '../third_party/webrtc/modules/modules.gyp:desktop_capture',
        '../ui/base/ui_base.gyp:ui_base',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        'remoting_base',
        'remoting_breakpad',
        'remoting_client',
        'remoting_client_plugin',
        'remoting_host',
        'remoting_host_setup_base',
        'remoting_it2me_host_static',
        'remoting_native_messaging_base',
        'remoting_protocol',
        'remoting_resources',
      ],
      'defines': [
        'VERSION=<(version_full)',
      ],
      'include_dirs': [
        '../testing/gmock/include',
      ],
      'sources': [
        '../chrome/test/base/run_all_remoting_unittests.cc',
        'base/auth_token_util_unittest.cc',
        'base/auto_thread_task_runner_unittest.cc',
        'base/auto_thread_unittest.cc',
        'base/breakpad_win_unittest.cc',
        'base/capabilities_unittest.cc',
        'base/compound_buffer_unittest.cc',
        'base/rate_counter_unittest.cc',
        'base/resources_unittest.cc',
        'base/rsa_key_pair_unittest.cc',
        'base/running_average_unittest.cc',
        'base/test_rsa_key_pair.h',
        'base/typed_buffer_unittest.cc',
        'base/util_unittest.cc',
        'client/audio_player_unittest.cc',
        'client/key_event_mapper_unittest.cc',
        'client/plugin/normalizing_input_filter_cros_unittest.cc',
        'client/plugin/normalizing_input_filter_mac_unittest.cc',
        'client/server_log_entry_client_unittest.cc',
        'codec/audio_encoder_opus_unittest.cc',
        'codec/codec_test.cc',
        'codec/codec_test.h',
        'codec/video_decoder_vpx_unittest.cc',
        'codec/video_encoder_verbatim_unittest.cc',
        'codec/video_encoder_vpx_unittest.cc',
        'host/audio_silence_detector_unittest.cc',
        'host/branding.cc',
        'host/branding.h',
        'host/capture_scheduler_unittest.cc',
        'host/chromoting_host_context_unittest.cc',
        'host/chromoting_host_unittest.cc',
        'host/client_session_unittest.cc',
        'host/config_file_watcher_unittest.cc',
        'host/daemon_process_unittest.cc',
        'host/desktop_process_unittest.cc',
        'host/desktop_shape_tracker_unittest.cc',
        'host/gnubby_auth_handler_posix_unittest.cc',
        'host/heartbeat_sender_unittest.cc',
        'host/host_change_notification_listener_unittest.cc',
        'host/host_mock_objects.cc',
        'host/host_status_monitor_fake.h',
        'host/host_status_sender_unittest.cc',
        'host/ipc_desktop_environment_unittest.cc',
        'host/it2me/it2me_native_messaging_host_unittest.cc',
        'host/json_host_config_unittest.cc',
        'host/linux/audio_pipe_reader_unittest.cc',
        'host/linux/unicode_to_keysym_unittest.cc',
        'host/linux/x_server_clipboard_unittest.cc',
        'host/local_input_monitor_unittest.cc',
        'host/log_to_server_unittest.cc',
        'host/native_messaging/native_messaging_reader_unittest.cc',
        'host/native_messaging/native_messaging_writer_unittest.cc',
        'host/pairing_registry_delegate_linux_unittest.cc',
        'host/pairing_registry_delegate_win_unittest.cc',
        'host/pin_hash_unittest.cc',
        'host/policy_hack/fake_policy_watcher.cc',
        'host/policy_hack/fake_policy_watcher.h',
        'host/policy_hack/mock_policy_callback.cc',
        'host/policy_hack/mock_policy_callback.h',
        'host/policy_hack/policy_watcher_unittest.cc',
        'host/register_support_host_request_unittest.cc',
        'host/remote_input_filter_unittest.cc',
        'host/resizing_host_observer_unittest.cc',
        'host/screen_capturer_fake.cc',
        'host/screen_capturer_fake.h',
        'host/screen_resolution_unittest.cc',
        'host/server_log_entry_host_unittest.cc',
        'host/setup/me2me_native_messaging_host.cc',
        'host/setup/me2me_native_messaging_host.h',
        'host/setup/me2me_native_messaging_host_unittest.cc',
        'host/setup/oauth_helper_unittest.cc',
        'host/setup/pin_validator_unittest.cc',
        'host/shaped_screen_capturer_unittest.cc',
        'host/token_validator_factory_impl_unittest.cc',
        'host/video_scheduler_unittest.cc',
        'host/win/rdp_client_unittest.cc',
        'host/win/worker_process_launcher.cc',
        'host/win/worker_process_launcher.h',
        'host/win/worker_process_launcher_unittest.cc',
        'jingle_glue/chromium_socket_factory_unittest.cc',
        'jingle_glue/fake_signal_strategy.cc',
        'jingle_glue/fake_signal_strategy.h',
        'jingle_glue/iq_sender_unittest.cc',
        'jingle_glue/mock_objects.cc',
        'jingle_glue/mock_objects.h',
        'jingle_glue/network_settings_unittest.cc',
        'jingle_glue/server_log_entry_unittest.cc',
        'jingle_glue/server_log_entry_unittest.h',
        'protocol/authenticator_test_base.cc',
        'protocol/authenticator_test_base.h',
        'protocol/buffered_socket_writer_unittest.cc',
        'protocol/channel_multiplexer_unittest.cc',
        'protocol/clipboard_echo_filter_unittest.cc',
        'protocol/clipboard_filter_unittest.cc',
        'protocol/connection_tester.cc',
        'protocol/connection_tester.h',
        'protocol/connection_to_client_unittest.cc',
        'protocol/content_description_unittest.cc',
        'protocol/fake_authenticator.cc',
        'protocol/fake_authenticator.h',
        'protocol/fake_session.cc',
        'protocol/fake_session.h',
        'protocol/input_event_tracker_unittest.cc',
        'protocol/input_filter_unittest.cc',
        'protocol/jingle_messages_unittest.cc',
        'protocol/jingle_session_unittest.cc',
        'protocol/message_decoder_unittest.cc',
        'protocol/message_reader_unittest.cc',
        'protocol/monitored_video_stub_unittest.cc',
        'protocol/mouse_input_filter_unittest.cc',
        'protocol/negotiating_authenticator_unittest.cc',
        'protocol/pairing_registry_unittest.cc',
        'protocol/ppapi_module_stub.cc',
        'protocol/protocol_mock_objects.cc',
        'protocol/protocol_mock_objects.h',
        'protocol/ssl_hmac_channel_authenticator_unittest.cc',
        'protocol/third_party_authenticator_unittest.cc',
        'protocol/v2_authenticator_unittest.cc',
      ],
      'conditions': [
        [ 'OS=="win"', {
          'defines': [
            '_ATL_NO_EXCEPTIONS',
          ],
          'include_dirs': [
            '../breakpad/src',
          ],
          'link_settings': {
            'libraries': [
              '-lrpcrt4.lib',
              '-lwtsapi32.lib',
            ],
          },
        }],
        [ 'OS=="mac" or (OS=="linux" and chromeos==0)', {
          # Javascript unittests are disabled on CrOS because they cause
          # valgrind and test errors.
          #
          # Javascript unittests are disabled on Windows because they add a
          # dependency on 'common_constants' which (only on Windows) requires
          # additional dependencies:
          #   '../content/content.gyp:content_common',
          #   'installer_util',
          # These targets are defined in .gypi files that would need to be
          # included here:
          #   '../chrome/chrome_common.gypi',
          #   '../chrome/chrome_installer.gypi',
          #   '../chrome/chrome_installer_util.gypi',
          # But we can't do that because ninja will complain about multiple
          # target definitions.
          # TODO(garykac): Move installer_util into a proper .gyp file so that
          # it can be included in multiple .gyp files.
          'includes': [
            '../chrome/js_unittest_rules.gypi',
          ],
          'dependencies': [
            '../chrome/common_constants.gyp:common_constants',
            '../v8/tools/gyp/v8.gyp:v8',
          ],
          'sources': [
            '../chrome/test/base/v8_unit_test.cc',
            '../chrome/test/base/v8_unit_test.h',
            'webapp/browser_globals.gtestjs',
            'webapp/all_js_load.gtestjs',
            'webapp/format_iq.gtestjs',
            '<@(remoting_webapp_all_js_files)',
          ],
        }],
        [ 'OS=="android"', {
          'dependencies!': [
            'remoting_client_plugin',
          ],
        }],
        [ 'OS=="android"', {
          'dependencies': [
            '../testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
        [ 'chromeos==0', {
          'sources!': [
            'client/plugin/normalizing_input_filter_cros_unittest.cc',
          ],
        }],
        ['enable_remoting_host == 0', {
          'dependencies!': [
            'remoting_host',
            'remoting_host_setup_base',
            'remoting_it2me_host_static',
            'remoting_native_messaging_base',
          ],
          'sources/': [
            ['exclude', '^codec/'],
            ['exclude', '^host/'],
            ['exclude', '^base/resources_unittest\\.cc$'],
          ]
        }],
        [ 'OS == "linux" and use_allocator!="none"', {
          'dependencies': [
            '../base/allocator/allocator.gyp:allocator',
          ],
        }],
      ],  # end of 'conditions'
    },  # end of target 'remoting_unittests'
    {
      'target_name': 'remoting_browser_test_resources',
      'type': 'none',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
            'files': [
              '<@(remoting_webapp_js_browser_test_files)',
            ],
        },
      ], #end of copies
    },  # end of target 'remoting_browser_test_resources'
  ],  # end of targets
}
