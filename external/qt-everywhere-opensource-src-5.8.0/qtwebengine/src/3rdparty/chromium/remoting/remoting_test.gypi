# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //remoting:test_support
      'target_name': 'remoting_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net_test_support',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'remoting_base',
        'remoting_client',
        'remoting_host',
        'remoting_protocol',
        'remoting_resources',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'host/fake_desktop_environment.cc',
        'host/fake_desktop_environment.h',
        'host/fake_host_extension.cc',
        'host/fake_host_extension.h',
        'host/fake_host_status_monitor.h',
        'host/fake_mouse_cursor_monitor.cc',
        'host/fake_mouse_cursor_monitor.h',
        'host/fake_oauth_token_getter.cc',
        'host/fake_oauth_token_getter.h',
        'host/security_key/fake_ipc_gnubby_auth_handler.cc',
        'host/security_key/fake_ipc_gnubby_auth_handler.h',
        'host/security_key/fake_remote_security_key_ipc_client.cc',
        'host/security_key/fake_remote_security_key_ipc_client.h',
        'host/security_key/fake_remote_security_key_ipc_server.cc',
        'host/security_key/fake_remote_security_key_ipc_server.h',
        'host/security_key/fake_remote_security_key_message_reader.cc',
        'host/security_key/fake_remote_security_key_message_reader.h',
        'host/security_key/fake_remote_security_key_message_writer.cc',
        'host/security_key/fake_remote_security_key_message_writer.h',
        'protocol/fake_authenticator.cc',
        'protocol/fake_authenticator.h',
        'protocol/fake_connection_to_client.cc',
        'protocol/fake_connection_to_client.h',
        'protocol/fake_connection_to_host.cc',
        'protocol/fake_connection_to_host.h',
        'protocol/fake_datagram_socket.cc',
        'protocol/fake_datagram_socket.h',
        'protocol/fake_desktop_capturer.cc',
        'protocol/fake_desktop_capturer.h',
        'protocol/fake_session.cc',
        'protocol/fake_session.h',
        'protocol/fake_stream_socket.cc',
        'protocol/fake_stream_socket.h',
        'protocol/fake_video_renderer.cc',
        'protocol/fake_video_renderer.h',
        'protocol/protocol_mock_objects.cc',
        'protocol/protocol_mock_objects.h',
        'protocol/test_event_matchers.h',
        'signaling/fake_signal_strategy.cc',
        'signaling/fake_signal_strategy.h',
        'signaling/mock_signal_strategy.cc',
        'signaling/mock_signal_strategy.h',
        'test/access_token_fetcher.cc',
        'test/access_token_fetcher.h',
        'test/app_remoting_report_issue_request.cc',
        'test/app_remoting_report_issue_request.h',
        'test/app_remoting_service_urls.cc',
        'test/app_remoting_service_urls.h',
        'test/chromoting_test_driver_environment.cc',
        'test/chromoting_test_driver_environment.h',
        'test/connection_setup_info.cc',
        'test/connection_setup_info.h',
        'test/connection_time_observer.cc',
        'test/connection_time_observer.h',
        'test/cyclic_frame_generator.cc',
        'test/cyclic_frame_generator.h',
        'test/fake_access_token_fetcher.cc',
        'test/fake_access_token_fetcher.h',
        'test/fake_app_remoting_report_issue_request.cc',
        'test/fake_app_remoting_report_issue_request.h',
        'test/fake_host_list_fetcher.cc',
        'test/fake_host_list_fetcher.h',
        'test/fake_network_dispatcher.cc',
        'test/fake_network_dispatcher.h',
        'test/fake_network_manager.cc',
        'test/fake_network_manager.h',
        'test/fake_port_allocator.cc',
        'test/fake_port_allocator.h',
        'test/fake_refresh_token_store.cc',
        'test/fake_refresh_token_store.h',
        'test/fake_remote_host_info_fetcher.cc',
        'test/fake_remote_host_info_fetcher.h',
        'test/fake_socket_factory.cc',
        'test/fake_socket_factory.h',
        'test/host_info.cc',
        'test/host_info.h',
        'test/host_list_fetcher.cc',
        'test/host_list_fetcher.h',
        'test/leaky_bucket.cc',
        'test/leaky_bucket.h',
        'test/mock_access_token_fetcher.cc',
        'test/mock_access_token_fetcher.h',
        'test/refresh_token_store.cc',
        'test/refresh_token_store.h',
        'test/remote_application_details.h',
        'test/remote_connection_observer.h',
        'test/remote_host_info.cc',
        'test/remote_host_info.h',
        'test/remote_host_info_fetcher.cc',
        'test/remote_host_info_fetcher.h',
        'test/rgb_value.cc',
        'test/rgb_value.h',
        'test/test_chromoting_client.cc',
        'test/test_chromoting_client.h',
        'test/test_video_renderer.cc',
        'test/test_video_renderer.h',
        'test/video_frame_writer.cc',
        'test/video_frame_writer.h',
      ],
      'conditions': [
        ['enable_remoting_host == 0', {
          'dependencies!': [
            'remoting_host',
          ],
          'sources/': [
            ['exclude', '^host/'],
          ]
        }],
        ['configuration_policy == 1', {
          'dependencies': [
            '../components/components.gyp:policy_component_test_support',
          ],
        }],
      ],
    },
    {
      'target_name': 'chromoting_test_driver',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../remoting/proto/chromotocol.gyp:chromotocol_proto_lib',
        '../testing/gtest.gyp:gtest',
        '../third_party/webrtc/modules/modules.gyp:desktop_capture',
        'remoting_test_support',
      ],
      'sources': [
        'test/chromoting_test_driver.cc',
        'test/chromoting_test_driver_tests.cc',
        'test/chromoting_test_fixture.cc',
        'test/chromoting_test_fixture.h',
      ],
    }, # end of target 'chromoting_test_driver'
    {
      'target_name': 'ar_test_driver_common',
      'type': 'static_library',
      'dependencies': [
        '../remoting/proto/chromotocol.gyp:chromotocol_proto_lib',
        '../testing/gtest.gyp:gtest',
        '../third_party/webrtc/modules/modules.gyp:desktop_capture',
        'remoting_test_support',
      ],
      'sources': [
        'test/app_remoting_connected_client_fixture.cc',
        'test/app_remoting_connected_client_fixture.h',
        'test/app_remoting_connection_helper.cc',
        'test/app_remoting_connection_helper.h',
        'test/app_remoting_latency_test_fixture.cc',
        'test/app_remoting_latency_test_fixture.h',
        'test/app_remoting_test_driver_environment.cc',
        'test/app_remoting_test_driver_environment.h',
      ],
    },  # end of target 'ar_test_driver_common'
    {
      # An external version of the test driver tool which includes minimal tests
      'target_name': 'ar_sample_test_driver',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'ar_test_driver_common',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'test/app_remoting_test_driver.cc',
        'test/app_remoting_sample_test_driver_environment.cc',
      ],
    },  # end of target 'ar_sample_test_driver'

    # Remoting unit tests
    {
      # GN version: //remoting:remoting_unittests
      # Note that many of the sources are broken out into subdir-specific unit
      # test source set targets that then GN version then brings together.
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
        'ar_test_driver_common',
        'remoting_base',
        'remoting_breakpad',
        'remoting_client',
        'remoting_host',
        'remoting_host_setup_base',
        'remoting_it2me_host_static',
        'remoting_native_messaging_base',
        'remoting_protocol',
        'remoting_resources',
        'remoting_test_support',
      ],
      'defines': [
        'VERSION=<(version_full)',
      ],
      'include_dirs': [
        '../testing/gmock/include',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'base/auto_thread_task_runner_unittest.cc',
        'base/auto_thread_unittest.cc',
        'base/breakpad_win_unittest.cc',
        'base/buffered_socket_writer_unittest.cc',
        'base/capabilities_unittest.cc',
        'base/compound_buffer_unittest.cc',
        'base/rate_counter_unittest.cc',
        'base/rsa_key_pair_unittest.cc',
        'base/run_all_unittests.cc',
        'base/running_samples_unittest.cc',
        'base/telemetry_log_writer_unittest.cc',
        'base/test_rsa_key_pair.h',
        'base/typed_buffer_unittest.cc',
        'base/util_unittest.cc',
        'client/audio_player_unittest.cc',
        'client/chromoting_client_runtime_unittest.cc',
        'client/client_status_logger_unittest.cc',
        'client/client_telemetry_logger_unittest.cc',
        'client/empty_cursor_filter_unittest.cc',
        'client/key_event_mapper_unittest.cc',
        'client/normalizing_input_filter_cros_unittest.cc',
        'client/normalizing_input_filter_mac_unittest.cc',
        'client/normalizing_input_filter_win_unittest.cc',
        'client/server_log_entry_client_unittest.cc',
        'client/software_video_renderer_unittest.cc',
        'client/touch_input_scaler_unittest.cc',
        'codec/audio_encoder_opus_unittest.cc',
        'codec/codec_test.cc',
        'codec/codec_test.h',
        'codec/video_decoder_vpx_unittest.cc',
        'codec/video_encoder_helper_unittest.cc',
        'codec/video_encoder_verbatim_unittest.cc',
        'codec/video_encoder_vpx_unittest.cc',
        'host/audio_pump_unittest.cc',
        'host/audio_silence_detector_unittest.cc',
        'host/backoff_timer_unittest.cc',
        'host/branding.cc',
        'host/branding.h',
        'host/chromeos/aura_desktop_capturer_unittest.cc',
        'host/chromeos/clipboard_aura_unittest.cc',
        'host/chromoting_host_context_unittest.cc',
        'host/chromoting_host_unittest.cc',
        'host/client_session_unittest.cc',
        'host/config_file_watcher_unittest.cc',
        'host/daemon_process_unittest.cc',
        'host/desktop_process_unittest.cc',
        'host/gcd_rest_client_unittest.cc',
        'host/gcd_state_updater_unittest.cc',
        'host/heartbeat_sender_unittest.cc',
        'host/host_change_notification_listener_unittest.cc',
        'host/host_config_unittest.cc',
        'host/host_extension_session_manager_unittest.cc',
        'host/host_mock_objects.cc',
        'host/host_status_logger_unittest.cc',
        'host/ipc_desktop_environment_unittest.cc',
        'host/it2me/it2me_confirmation_dialog_proxy_unittest.cc',
        'host/it2me/it2me_native_messaging_host_unittest.cc',
        'host/linux/audio_pipe_reader_unittest.cc',
        'host/linux/certificate_watcher_unittest.cc',
        'host/linux/unicode_to_keysym_unittest.cc',
        'host/linux/x_server_clipboard_unittest.cc',
        'host/local_input_monitor_unittest.cc',
        'host/mouse_cursor_monitor_proxy_unittest.cc',
        'host/mouse_shape_pump_unittest.cc',
        'host/native_messaging/native_messaging_reader_unittest.cc',
        'host/native_messaging/native_messaging_writer_unittest.cc',
        'host/pairing_registry_delegate_linux_unittest.cc',
        'host/pairing_registry_delegate_win_unittest.cc',
        'host/pin_hash_unittest.cc',
        'host/policy_watcher_unittest.cc',
        'host/register_support_host_request_unittest.cc',
        'host/remote_input_filter_unittest.cc',
        'host/resizing_host_observer_unittest.cc',
        'host/resources_unittest.cc',
        'host/screen_resolution_unittest.cc',
        'host/security_key/gnubby_auth_handler_linux_unittest.cc',
        'host/security_key/gnubby_auth_handler_win_unittest.cc',
        'host/security_key/gnubby_extension_session_unittest.cc',
        'host/security_key/remote_security_key_ipc_client_unittest.cc',
        'host/security_key/remote_security_key_ipc_constants.cc',
        'host/security_key/remote_security_key_ipc_server_unittest.cc',
        'host/security_key/remote_security_key_message_handler_unittest.cc',
        'host/security_key/remote_security_key_message_reader_impl_unittest.cc',
        'host/security_key/remote_security_key_message_writer_impl_unittest.cc',
        'host/server_log_entry_host_unittest.cc',
        'host/setup/me2me_native_messaging_host.cc',
        'host/setup/me2me_native_messaging_host.h',
        'host/setup/me2me_native_messaging_host_unittest.cc',
        'host/setup/mock_oauth_client.cc',
        'host/setup/mock_oauth_client.h',
        'host/setup/oauth_helper_unittest.cc',
        'host/setup/pin_validator_unittest.cc',
        'host/third_party_auth_config_unittest.cc',
        'host/token_validator_factory_impl_unittest.cc',
        'host/touch_injector_win_unittest.cc',
        'host/win/rdp_client_unittest.cc',
        'host/win/worker_process_launcher.cc',
        'host/win/worker_process_launcher.h',
        'host/win/worker_process_launcher_unittest.cc',
        'protocol/authenticator_test_base.cc',
        'protocol/authenticator_test_base.h',
        'protocol/capture_scheduler_unittest.cc',
        'protocol/channel_multiplexer_unittest.cc',
        'protocol/channel_socket_adapter_unittest.cc',
        'protocol/chromium_socket_factory_unittest.cc',
        'protocol/client_video_dispatcher_unittest.cc',
        'protocol/clipboard_echo_filter_unittest.cc',
        'protocol/clipboard_filter_unittest.cc',
        'protocol/connection_tester.cc',
        'protocol/connection_tester.h',
        'protocol/connection_unittest.cc',
        'protocol/content_description_unittest.cc',
        'protocol/http_ice_config_request_unittest.cc',
        'protocol/ice_transport_unittest.cc',
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
        'protocol/port_range_unittest.cc',
        'protocol/ppapi_module_stub.cc',
        'protocol/pseudotcp_adapter_unittest.cc',
        'protocol/session_config_unittest.cc',
        'protocol/spake2_authenticator_unittest.cc',
        'protocol/ssl_hmac_channel_authenticator_unittest.cc',
        'protocol/third_party_authenticator_unittest.cc',
        'protocol/v2_authenticator_unittest.cc',
        'protocol/video_frame_pump_unittest.cc',
        'protocol/webrtc_transport_unittest.cc',
        'signaling/iq_sender_unittest.cc',
        'signaling/jid_util_unittest.cc',
        'signaling/log_to_server_unittest.cc',
        'signaling/push_notification_subscriber_unittest.cc',
        'signaling/server_log_entry_unittest.cc',
        'signaling/server_log_entry_unittest.h',
        'signaling/xmpp_login_handler_unittest.cc',
        'signaling/xmpp_signal_strategy_unittest.cc',
        'signaling/xmpp_stream_parser_unittest.cc',
        'test/access_token_fetcher_unittest.cc',
        'test/app_remoting_report_issue_request_unittest.cc',
        'test/app_remoting_test_driver_environment_unittest.cc',
        'test/chromoting_test_driver_environment_unittest.cc',
        'test/connection_time_observer_unittest.cc',
        'test/host_list_fetcher_unittest.cc',
        'test/remote_host_info_fetcher_unittest.cc',
        'test/test_chromoting_client_unittest.cc',
        'test/test_video_renderer_unittest.cc',
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
        [ 'chromeos==0', {
          'sources!': [
            'host/chromeos/aura_desktop_capturer_unittest.cc',
            'host/clipboard_aura_unittest.cc',
          ],
        }, { # chromeos==1
          'sources!': [
            'host/linux/x_server_clipboard_unittest.cc',
            'host/local_input_monitor_unittest.cc',
          ],
        }],
        ['use_x11 == 0', {
          'sources!' : [
            'host/linux/unicode_to_keysym_unittest.cc',
          ]
        }],
        [ 'use_ozone==1', {
          'sources!': [
            'host/local_input_monitor_unittest.cc',
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
          ]
        }],
        ['configuration_policy == 1', {
          'dependencies': [
            '../components/components.gyp:policy',
          ],
        }],
      ],  # end of 'conditions'
    },  # end of target 'remoting_unittests'
    {
      # GN version: //remoting/webapp:browser_test_resources
      'target_name': 'remoting_browser_test_resources',
      'type': 'none',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/remoting/browser_test_resources',
            'files': [
              '<@(remoting_webapp_browsertest_main_html_extra_files)',
            ],
        },
      ], # end of copies
    },  # end of target 'remoting_browser_test_resources'
    {
      'target_name': 'remoting_webapp_browser_test_main_html',
      'type': 'none',
      'actions': [
        {
          'action_name': 'Build Remoting Webapp Browser test main.html',
          'inputs': [
            'webapp/build-html.py',
            '<(remoting_webapp_template_main)',
            '<@(remoting_webapp_template_files)',
            '<@(remoting_webapp_crd_main_html_all_js_files)',
            '<@(remoting_webapp_browsertest_main_html_extra_files)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/browser_test/main.html',
          ],
          'action': [
            'python', 'webapp/build-html.py',
            '<(SHARED_INTERMEDIATE_DIR)/browser_test/main.html',
            '<(remoting_webapp_template_main)',
            '--template-dir', '<(DEPTH)/remoting',
            '--templates', '<@(remoting_webapp_template_files)',
            '--js',
            '<@(remoting_webapp_crd_main_html_all_js_files)',
            '<@(remoting_webapp_browsertest_main_html_extra_files)',
          ],
        },
      ],  # end of actions
    },  # end of target 'remoting_webapp_browser_test_html'
    {
      # GN version: //remoting/webapp:unit_tests
      'target_name': 'remoting_webapp_unittests',
      'type': 'none',
      'variables': {
        'output_dir': '<(PRODUCT_DIR)/remoting/unittests',
        'webapp_js_files': [
          '<@(remoting_webapp_unittest_html_all_js_files)',
          '<@(remoting_webapp_wcs_sandbox_html_js_files)',
          '<@(remoting_webapp_background_html_js_files)',
        ]
      },
      'copies': [
        {
          # GN version: //remoting/webapp:qunit
          'destination': '<(output_dir)/qunit',
          'files': [
            '../third_party/qunit/src/browser_test_harness.js',
            '../third_party/qunit/src/qunit.css',
            '../third_party/qunit/src/qunit.js',
          ],
        },
        {
          # GN version: //remoting/webapp:blanketjs
          'destination': '<(output_dir)/blanketjs',
          'files': [
            '../third_party/blanketjs/src/blanket.js',
            '../third_party/blanketjs/src/qunit_adapter.js',
          ],
        },
        {
          # GN version: //remoting/webapp:sinonjs
          'destination': '<(output_dir)/sinonjs',
          'files': [
            '../third_party/sinonjs/src/sinon.js',
            '../third_party/sinonjs/src/sinon-qunit.js',
          ],
        },
        {
          # GN version: //remoting/webapp:js_files
          'destination': '<(output_dir)',
          'files': [
            '<@(webapp_js_files)',
            '<@(remoting_webapp_unittests_all_files)',
          ],
        },
      ],
      'actions': [
        {
          'action_name': 'Build Remoting Webapp unittests.html',
          'inputs': [
            'webapp/build-html.py',
            '<(remoting_webapp_unittests_template_main)',
            '<@(webapp_js_files)',
            '<@(remoting_webapp_unittests_all_js_files)'
          ],
          'outputs': [
            '<(output_dir)/unittests.html',
          ],
          'action': [
            'python', 'webapp/build-html.py',
            '<@(_outputs)',
            '<(remoting_webapp_unittests_template_main)',
            # GYP automatically removes subsequent duplicated command line
            # arguments.  Therefore, the excludejs flag must be set before the
            # instrumentedjs flag or else GYP will ignore the files in the
            # exclude list.
            '--exclude-js', '<@(remoting_webapp_unittests_exclude_js_files)',
            '--js', '<@(remoting_webapp_unittests_all_js_files)',
            '--instrument-js', '<@(webapp_js_files)',
           ],
        },
      ],
    },  # end of target 'remoting_webapp_unittest'
  ],  # end of targets

  'conditions': [
    ['enable_remoting_host==1', {
      'targets': [
        # Remoting performance tests
        {
          'target_name': 'remoting_perftests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
            '../testing/gtest.gyp:gtest',
            '../third_party/webrtc/modules/modules.gyp:desktop_capture',
            '../third_party/libjingle/libjingle.gyp:libjingle',
            'remoting_base',
            'remoting_test_support',
          ],
          'defines': [
            'VERSION=<(version_full)',
          ],
          'include_dirs': [
            '../testing/gmock/include',
          ],
          'sources': [
            'base/run_all_unittests.cc',
            'test/codec_perftest.cc',
            'test/protocol_perftest.cc',
          ],
          'conditions': [
            [ 'OS=="mac" or (OS=="linux" and chromeos==0)', {
              # RunAllTests calls chrome::RegisterPathProvider() under Mac and
              # Linux, so we need the chrome_common.gypi dependency.
              'dependencies': [
                '../chrome/common_constants.gyp:common_constants',
              ],
            }],
          ],  # end of 'conditions'
        },  # end of target 'remoting_perftests'
      ]
    }],  # end of 'enable_remoting_host'
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'remoting_webapp_browser_test',
          'type': 'none',
          'variables': {
            'output_dir': '<(PRODUCT_DIR)/remoting/remoting.webapp.browsertest.v2',
            'zip_path': '<(PRODUCT_DIR)/remoting-webapp.browsertest.v2.zip',
            'main_html_file': '<(SHARED_INTERMEDIATE_DIR)/browser_test/main.html',
            'extra_files': [
              'webapp/crd/remoting_client_pnacl.nmf.jinja2',
              '<(PRODUCT_DIR)/remoting_client_plugin_newlib.pexe',
              '<@(remoting_webapp_browsertest_main_html_extra_files)',
            ],
          },
          'dependencies': [
            'remoting_nacl.gyp:remoting_client_plugin_nacl',
            'remoting_webapp_browser_test_main_html',
          ],
          'includes': [ 'remoting_webapp.gypi', ],
        },  # end of target 'remoting_webapp_browser_test'
      ]
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'remoting_unittests_run',
          'type': 'none',
          'dependencies': [
            'remoting_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'remoting_unittests.isolate',
          ],
        },
      ],
    }],
  ] # end of 'conditions'
}
