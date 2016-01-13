# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'remoting_host_linux.gypi',
    'remoting_host_mac.gypi',
    'remoting_host_win.gypi',
  ],

  'variables': {
    'conditions': [
      # Remoting host is supported only on Windows, OSX and Linux (with X11).
      ['OS=="win" or OS=="mac" or (OS=="linux" and chromeos==0 and use_x11==1)', {
        'enable_remoting_host': 1,
      }, {
        'enable_remoting_host': 0,
      }],
    ],
  },

  'conditions': [
    ['enable_remoting_host==1', {
      'targets': [
        {
          'target_name': 'remoting_host',
          'type': 'static_library',
          'variables': {
            'enable_wexit_time_destructors': 1,
            'host_bundle_name': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_HOST_BUNDLE_NAME@")',
            'prefpane_bundle_name': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_PREFPANE_BUNDLE_NAME@")',
          },
          'dependencies': [
            'remoting_base',
            'remoting_protocol',
            'remoting_resources',
            '../base/base.gyp:base_i18n',
            '../crypto/crypto.gyp:crypto',
            '../google_apis/google_apis.gyp:google_apis',
            '../ipc/ipc.gyp:ipc',
            '../third_party/webrtc/modules/modules.gyp:desktop_capture',
            '../ui/events/events.gyp:dom4_keycode_converter',
          ],
          'defines': [
            'HOST_BUNDLE_NAME="<(host_bundle_name)"',
            'PREFPANE_BUNDLE_NAME="<(prefpane_bundle_name)"',
            'VERSION=<(version_full)',
          ],
          'sources': [
            'host/audio_capturer.cc',
            'host/audio_capturer.h',
            'host/audio_capturer_linux.cc',
            'host/audio_capturer_linux.h',
            'host/audio_capturer_mac.cc',
            'host/audio_capturer_win.cc',
            'host/audio_capturer_win.h',
            'host/audio_scheduler.cc',
            'host/audio_scheduler.h',
            'host/audio_silence_detector.cc',
            'host/audio_silence_detector.h',
            'host/basic_desktop_environment.cc',
            'host/basic_desktop_environment.h',
            'host/branding.cc',
            'host/branding.h',
            'host/capture_scheduler.cc',
            'host/capture_scheduler.h',
            'host/chromoting_host.cc',
            'host/chromoting_host.h',
            'host/chromoting_host_context.cc',
            'host/chromoting_host_context.h',
            'host/chromoting_messages.cc',
            'host/chromoting_messages.h',
            'host/chromoting_param_traits.cc',
            'host/chromoting_param_traits.h',
            'host/client_session.cc',
            'host/client_session.h',
            'host/client_session_control.h',
            'host/clipboard.h',
            'host/clipboard_mac.mm',
            'host/clipboard_win.cc',
            'host/clipboard_x11.cc',
            'host/config_file_watcher.cc',
            'host/config_file_watcher.h',
            'host/config_watcher.h',
            'host/constants_mac.cc',
            'host/constants_mac.h',
            'host/continue_window.cc',
            'host/continue_window.h',
            'host/continue_window_linux.cc',
            'host/continue_window_mac.mm',
            'host/continue_window_win.cc',
            'host/daemon_process.cc',
            'host/daemon_process.h',
            'host/daemon_process_win.cc',
            'host/desktop_environment.h',
            'host/desktop_process.cc',
            'host/desktop_process.h',
            'host/desktop_resizer.h',
            'host/desktop_resizer_linux.cc',
            'host/desktop_session.cc',
            'host/desktop_session.h',
            'host/desktop_session_agent.cc',
            'host/desktop_session_agent.h',
            'host/desktop_session_win.cc',
            'host/desktop_session_win.h',
            'host/desktop_resizer_mac.cc',
            'host/desktop_resizer_win.cc',
            'host/desktop_session_connector.h',
            'host/desktop_session_proxy.cc',
            'host/desktop_session_proxy.h',
            'host/desktop_shape_tracker.h',
            'host/desktop_shape_tracker_mac.cc',
            'host/desktop_shape_tracker_win.cc',
            'host/desktop_shape_tracker_x11.cc',
            'host/disconnect_window_linux.cc',
            'host/disconnect_window_mac.h',
            'host/disconnect_window_mac.mm',
            'host/disconnect_window_win.cc',
            'host/dns_blackhole_checker.cc',
            'host/dns_blackhole_checker.h',
            'host/gnubby_auth_handler.h',
            'host/gnubby_auth_handler_posix.cc',
            'host/gnubby_auth_handler_posix.h',
            'host/gnubby_auth_handler_win.cc',
            'host/gnubby_socket.cc',
            'host/gnubby_socket.h',
            'host/heartbeat_sender.cc',
            'host/heartbeat_sender.h',
            'host/host_change_notification_listener.cc',
            'host/host_change_notification_listener.h',
            'host/host_config.cc',
            'host/host_config.h',
            'host/host_event_logger.h',
            'host/host_event_logger_posix.cc',
            'host/host_event_logger_win.cc',
            'host/host_exit_codes.cc',
            'host/host_exit_codes.h',
            'host/host_export.h',
            'host/host_extension.h',
            'host/host_extension_session.h',
            'host/host_secret.cc',
            'host/host_secret.h',
            'host/host_status_monitor.h',
            'host/host_status_observer.h',
            'host/host_status_sender.cc',
            'host/host_status_sender.h',
            'host/host_window.h',
            'host/host_window_proxy.cc',
            'host/host_window_proxy.h',
            'host/in_memory_host_config.cc',
            'host/in_memory_host_config.h',
            'host/input_injector.h',
            'host/input_injector_linux.cc',
            'host/input_injector_mac.cc',
            'host/input_injector_win.cc',
            'host/ipc_audio_capturer.cc',
            'host/ipc_audio_capturer.h',
            'host/ipc_constants.cc',
            'host/ipc_constants.h',
            'host/ipc_desktop_environment.cc',
            'host/ipc_desktop_environment.h',
            'host/ipc_host_event_logger.cc',
            'host/ipc_host_event_logger.h',
            'host/ipc_input_injector.cc',
            'host/ipc_input_injector.h',
            'host/ipc_screen_controls.cc',
            'host/ipc_screen_controls.h',
            'host/ipc_util.h',
            'host/ipc_util_posix.cc',
            'host/ipc_util_win.cc',
            'host/ipc_video_frame_capturer.cc',
            'host/ipc_video_frame_capturer.h',
            'host/it2me_desktop_environment.cc',
            'host/it2me_desktop_environment.h',
            'host/json_host_config.cc',
            'host/json_host_config.h',
            'host/linux/audio_pipe_reader.cc',
            'host/linux/audio_pipe_reader.h',
            'host/linux/unicode_to_keysym.cc',
            'host/linux/unicode_to_keysym.h',
            'host/linux/x11_util.cc',
            'host/linux/x11_util.h',
            'host/linux/x_server_clipboard.cc',
            'host/linux/x_server_clipboard.h',
            'host/local_input_monitor.h',
            'host/local_input_monitor_linux.cc',
            'host/local_input_monitor_mac.mm',
            'host/local_input_monitor_win.cc',
            'host/log_to_server.cc',
            'host/log_to_server.h',
            'host/logging.h',
            'host/logging_posix.cc',
            'host/logging_win.cc',
            'host/me2me_desktop_environment.cc',
            'host/me2me_desktop_environment.h',
            'host/mouse_clamping_filter.cc',
            'host/mouse_clamping_filter.h',
            'host/oauth_token_getter.cc',
            'host/oauth_token_getter.h',
            'host/pairing_registry_delegate.cc',
            'host/pairing_registry_delegate.h',
            'host/pairing_registry_delegate_linux.cc',
            'host/pairing_registry_delegate_linux.h',
            'host/pairing_registry_delegate_mac.cc',
            'host/pairing_registry_delegate_win.cc',
            'host/pairing_registry_delegate_win.h',
            'host/pam_authorization_factory_posix.cc',
            'host/pam_authorization_factory_posix.h',
            'host/pin_hash.cc',
            'host/pin_hash.h',
            'host/policy_hack/policy_watcher.cc',
            'host/policy_hack/policy_watcher.h',
            'host/policy_hack/policy_watcher_linux.cc',
            'host/policy_hack/policy_watcher_mac.mm',
            'host/policy_hack/policy_watcher_win.cc',
            'host/register_support_host_request.cc',
            'host/register_support_host_request.h',
            'host/remote_input_filter.cc',
            'host/remote_input_filter.h',
            'host/resizing_host_observer.cc',
            'host/resizing_host_observer.h',
            'host/sas_injector.h',
            'host/sas_injector_win.cc',
            'host/screen_controls.h',
            'host/screen_resolution.cc',
            'host/screen_resolution.h',
            'host/server_log_entry_host.cc',
            'host/server_log_entry_host.h',
            'host/service_urls.cc',
            'host/service_urls.h',
            'host/session_manager_factory.cc',
            'host/session_manager_factory.h',
            'host/shaped_screen_capturer.cc',
            'host/shaped_screen_capturer.h',
            'host/signaling_connector.cc',
            'host/signaling_connector.h',
            'host/token_validator_base.cc',
            'host/token_validator_base.h',
            'host/token_validator_factory_impl.cc',
            'host/token_validator_factory_impl.h',
            'host/usage_stats_consent.h',
            'host/usage_stats_consent_mac.cc',
            'host/usage_stats_consent_win.cc',
            'host/username.cc',
            'host/username.h',
            'host/video_scheduler.cc',
            'host/video_scheduler.h',
            'host/win/com_imported_mstscax.tlh',
            'host/win/com_security.cc',
            'host/win/com_security.h',
            'host/win/launch_process_with_token.cc',
            'host/win/launch_process_with_token.h',
            'host/win/omaha.cc',
            'host/win/omaha.h',
            'host/win/rdp_client.cc',
            'host/win/rdp_client.h',
            'host/win/rdp_client_window.cc',
            'host/win/rdp_client_window.h',
            'host/win/security_descriptor.cc',
            'host/win/security_descriptor.h',
            'host/win/session_desktop_environment.cc',
            'host/win/session_desktop_environment.h',
            'host/win/session_input_injector.cc',
            'host/win/session_input_injector.h',
            'host/win/window_station_and_desktop.cc',
            'host/win/window_station_and_desktop.h',
            'host/win/wts_terminal_monitor.cc',
            'host/win/wts_terminal_monitor.h',
            'host/win/wts_terminal_observer.h',
          ],
          'conditions': [
            ['OS=="linux"', {
              'dependencies': [
                # Always use GTK on Linux, even for Aura builds.
                '../build/linux/system.gyp:gtk',
                '../build/linux/system.gyp:x11',
                '../build/linux/system.gyp:xext',
                '../build/linux/system.gyp:xfixes',
                '../build/linux/system.gyp:xi',
                '../build/linux/system.gyp:xrandr',
                '../build/linux/system.gyp:xtst',
              ],
              'link_settings': {
                'libraries': [
                  '-lpam',
                ],
              },
            }],
            ['OS=="mac"', {
              'dependencies': [
                '../third_party/google_toolbox_for_mac/google_toolbox_for_mac.gyp:google_toolbox_for_mac',
              ],
              'link_settings': {
                'libraries': [
                  '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
                  'libpam.a',
               ],
              },
            }],
            ['OS=="win"', {
              'defines': [
                '_ATL_NO_EXCEPTIONS',
                'ISOLATION_AWARE_ENABLED=1',
              ],
              'dependencies': [
                '../sandbox/sandbox.gyp:sandbox',
                'remoting_host_messages',
                'remoting_lib_idl',
              ],
              'msvs_settings': {
                'VCCLCompilerTool': {
                  # /MP conflicts with #import directive so we limit the number
                  # of processes to spawn to 1.
                  'AdditionalOptions': ['/MP1'],
                },
              },
              'variables': {
                'output_dir': '<(SHARED_INTERMEDIATE_DIR)/remoting/host',
              },
              'sources': [
                '<(output_dir)/remoting_host_messages.mc',
              ],
              'include_dirs': [
                '<(output_dir)',
              ],
              'direct_dependent_settings': {
                'include_dirs': [
                  '<(output_dir)',
                ],
              },
              'rules': [{
                # Rule to run the message compiler.
                'rule_name': 'message_compiler',
                'extension': 'mc',
                'outputs': [
                  '<(output_dir)/<(RULE_INPUT_ROOT).h',
                  '<(output_dir)/<(RULE_INPUT_ROOT).rc',
                ],
                'action': [
                  'mc.exe',
                  '-h', '<(output_dir)',
                  '-r', '<(output_dir)/.',
                  '-u',
                  '<(RULE_INPUT_PATH)',
                ],
                'process_outputs_as_sources': 1,
                'message': 'Running message compiler on <(RULE_INPUT_PATH)',
              }],
            }],
          ],
        },  # end of target 'remoting_host'

        {
          'target_name': 'remoting_native_messaging_base',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            '../base/base.gyp:base',
          ],
          'sources': [
            'host/native_messaging/native_messaging_channel.cc',
            'host/native_messaging/native_messaging_channel.h',
            'host/native_messaging/native_messaging_reader.cc',
            'host/native_messaging/native_messaging_reader.h',
            'host/native_messaging/native_messaging_writer.cc',
            'host/native_messaging/native_messaging_writer.h',
          ],
        },  # end of target 'remoting_native_messaging_base'

        {
          'target_name': 'remoting_me2me_host_static',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:base_i18n',
            '../net/net.gyp:net',
            '../third_party/webrtc/modules/modules.gyp:desktop_capture',
            'remoting_base',
            'remoting_breakpad',
            'remoting_host',
            'remoting_protocol',
          ],
          'defines': [
            'VERSION=<(version_full)',
          ],
          'sources': [
            'host/curtain_mode.h',
            'host/curtain_mode_linux.cc',
            'host/curtain_mode_mac.cc',
            'host/curtain_mode_win.cc',
            'host/posix/signal_handler.cc',
            'host/posix/signal_handler.h',
          ],
          'conditions': [
            ['os_posix != 1', {
              'sources/': [
                ['exclude', '^host/posix/'],
              ],
            }],
          ],  # end of 'conditions'
        },  # end of target 'remoting_me2me_host_static'

        {
          'target_name': 'remoting_host_setup_base',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            '../base/base.gyp:base',
            '../google_apis/google_apis.gyp:google_apis',
            'remoting_host',
          ],
          'defines': [
            'VERSION=<(version_full)',
          ],
          'sources': [
            'host/setup/daemon_controller.cc',
            'host/setup/daemon_controller.h',
            'host/setup/daemon_controller_delegate_linux.cc',
            'host/setup/daemon_controller_delegate_linux.h',
            'host/setup/daemon_controller_delegate_mac.h',
            'host/setup/daemon_controller_delegate_mac.mm',
            'host/setup/daemon_controller_delegate_win.cc',
            'host/setup/daemon_controller_delegate_win.h',
            'host/setup/daemon_installer_win.cc',
            'host/setup/daemon_installer_win.h',
            'host/setup/oauth_client.cc',
            'host/setup/oauth_client.h',
            'host/setup/oauth_helper.cc',
            'host/setup/oauth_helper.h',
            'host/setup/pin_validator.cc',
            'host/setup/pin_validator.h',
            'host/setup/service_client.cc',
            'host/setup/service_client.h',
            'host/setup/test_util.cc',
            'host/setup/test_util.h',
            'host/setup/win/auth_code_getter.cc',
            'host/setup/win/auth_code_getter.h',
          ],
          'conditions': [
            ['OS=="win"', {
              'dependencies': [
                '../google_update/google_update.gyp:google_update',
                'remoting_lib_idl',
              ],
              # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
              'msvs_disabled_warnings': [4267, ],
            }],
          ],
        },  # end of target 'remoting_host_setup_base'

        {
          'target_name': 'remoting_it2me_host_static',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            '../base/base.gyp:base_i18n',
            '../net/net.gyp:net',
            'remoting_base',
            'remoting_host',
            'remoting_infoplist_strings',
            'remoting_protocol',
            'remoting_resources',
          ],
          'defines': [
            'VERSION=<(version_full)',
          ],
          'sources': [
            'host/it2me/it2me_host.cc',
            'host/it2me/it2me_host.h',
            'host/it2me/it2me_native_messaging_host.cc',
            'host/it2me/it2me_native_messaging_host.h',
          ],
        },  # end of target 'remoting_it2me_host_static'

        # Generates native messaging manifest files.
        {
          'target_name': 'remoting_native_messaging_manifests',
          'type': 'none',
          'conditions': [
            [ 'OS == "win"', {
              'variables': {
                'me2me_host_path': 'remoting_native_messaging_host.exe',
                'it2me_host_path': 'remote_assistance_host.exe',
              },
            }],
            [ 'OS == "mac"', {
              'variables': {
                'me2me_host_bundle_name': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_HOST_BUNDLE_NAME@")',
                'native_messaging_host_bundle_name': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_NATIVE_MESSAGING_HOST_BUNDLE_NAME@")',
                'remote_assistance_host_bundle_name': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_REMOTE_ASSISTANCE_HOST_BUNDLE_NAME@")',
                'me2me_host_path':
                    '/Library/PrivilegedHelperTools/<(me2me_host_bundle_name)/Contents/MacOS/<(native_messaging_host_bundle_name)/Contents/MacOS/native_messaging_host',
                'it2me_host_path':
                    '/Library/PrivilegedHelperTools/<(me2me_host_bundle_name)/Contents/MacOS/<(remote_assistance_host_bundle_name)/Contents/MacOS/remote_assistance_host',
              },
            }],
            [ 'OS != "mac" and OS != "win"', {
              'variables': {
                'me2me_host_path':
                    '/opt/google/chrome-remote-desktop/native-messaging-host',
                'it2me_host_path':
                    '/opt/google/chrome-remote-desktop/remote-assistance-host',
              },
            }],
          ],  # conditions
          'sources': [
            'host/it2me/com.google.chrome.remote_assistance.json.jinja2',
            'host/setup/com.google.chrome.remote_desktop.json.jinja2',
          ],
          'rules': [{
            'rule_name': 'generate_manifest',
            'extension': 'jinja2',
            'inputs': [
              '<(remoting_localize_path)',
              '<(branding_path)',
            ],
            'outputs': [
              '<(PRODUCT_DIR)/remoting/<(RULE_INPUT_ROOT)',
            ],
            'action': [
              'python', '<(remoting_localize_path)',
              '--define', 'ME2ME_HOST_PATH=<(me2me_host_path)',
              '--define', 'IT2ME_HOST_PATH=<(it2me_host_path)',
              '--variables', '<(branding_path)',
              '--template', '<(RULE_INPUT_PATH)',
              '--locale_output', '<@(_outputs)',
              'en',
            ],
          }],
        },  # end of target 'remoting_native_messaging_manifests'

        {
          'target_name': 'remoting_infoplist_strings',
          'type': 'none',
          'dependencies': [
            'remoting_resources',
          ],
          'sources': [
            'host/remoting_me2me_host-InfoPlist.strings.jinja2',
            'host/mac/me2me_preference_pane-InfoPlist.strings.jinja2',
            'host/installer/mac/uninstaller/remoting_uninstaller-InfoPlist.strings.jinja2',
            'host/setup/native_messaging_host-InfoPlist.strings.jinja2',
            'host/it2me/remote_assistance_host-InfoPlist.strings.jinja2',
          ],
          'rules': [{
            'rule_name': 'generate_strings',
            'extension': 'jinja2',
            'inputs': [
              '<(remoting_localize_path)',
            ],
            'outputs': [
              '<!@pymod_do_main(remoting_localize --locale_output '
                  '"<(SHARED_INTERMEDIATE_DIR)/remoting/<(RULE_INPUT_ROOT)/@{json_suffix}.lproj/InfoPlist.strings" '
                  '--print_only <(remoting_locales))',
            ],
            'action': [
              'python', '<(remoting_localize_path)',
              '--locale_dir', '<(webapp_locale_dir)',
              '--template', '<(RULE_INPUT_PATH)',
              '--locale_output',
              '<(SHARED_INTERMEDIATE_DIR)/remoting/<(RULE_INPUT_ROOT)/@{json_suffix}.lproj/InfoPlist.strings',
              '<@(remoting_locales)',
            ]},
          ],
        },  # end of target 'remoting_infoplist_strings'
      ],  # end of 'targets'
    }],  # 'enable_remoting_host==1'

    ['OS!="win" and enable_remoting_host==1', {
      'targets': [
        {
          'target_name': 'remoting_me2me_host',
          'type': 'executable',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:base_i18n',
            '<(icu_gyp_path):icudata',
            '../net/net.gyp:net',
            '../third_party/webrtc/modules/modules.gyp:desktop_capture',
            'remoting_base',
            'remoting_breakpad',
            'remoting_host',
            'remoting_infoplist_strings',
            'remoting_me2me_host_static',
            'remoting_protocol',
          ],
          'defines': [
            'VERSION=<(version_full)',
          ],
          'sources': [
            'host/host_main.cc',
            'host/host_main.h',
            'host/remoting_me2me_host.cc',
          ],
          'conditions': [
            ['OS=="mac"', {
              'mac_bundle': 1,
              'variables': {
                 'host_bundle_id': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_HOST_BUNDLE_ID@")',
              },
              'xcode_settings': {
                'INFOPLIST_FILE': 'host/remoting_me2me_host-Info.plist',
                'INFOPLIST_PREPROCESS': 'YES',
                'INFOPLIST_PREPROCESSOR_DEFINITIONS': 'VERSION_FULL="<(version_full)" VERSION_SHORT="<(version_short)" BUNDLE_ID="<(host_bundle_id)"',
              },
              'mac_bundle_resources': [
                '<(PRODUCT_DIR)/icudtl.dat',
                'host/disconnect_window.xib',
                'host/remoting_me2me_host.icns',
                'host/remoting_me2me_host-Info.plist',
                '<!@pymod_do_main(remoting_copy_locales -o -p <(OS) -x <(PRODUCT_DIR) <(remoting_locales))',

                # Localized strings for 'Info.plist'
                '<!@pymod_do_main(remoting_localize --locale_output '
                    '"<(SHARED_INTERMEDIATE_DIR)/remoting/remoting_me2me_host-InfoPlist.strings/@{json_suffix}.lproj/InfoPlist.strings" '
                    '--print_only <(remoting_locales))',
              ],
              'mac_bundle_resources!': [
                'host/remoting_me2me_host-Info.plist',
              ],
              'conditions': [
                ['mac_breakpad==1', {
                  'variables': {
                    # A real .dSYM is needed for dump_syms to operate on.
                    'mac_real_dsym': 1,
                  },
                  'copies': [
                    {
                      'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Resources',
                      'files': [
                        '<(PRODUCT_DIR)/crash_inspector',
                        '<(PRODUCT_DIR)/crash_report_sender.app'
                      ],
                    },
                  ],
                  'dependencies': [
                    '../breakpad/breakpad.gyp:dump_syms',
                  ],
                  'postbuilds': [
                    {
                      'postbuild_name': 'Dump Symbols',
                      'variables': {
                        'dump_product_syms_path':
                            'scripts/mac/dump_product_syms',
                      },
                      'action': [
                        '<(dump_product_syms_path)',
                        '<(version_full)',
                      ],
                    },  # end of postbuild 'dump_symbols'
                  ],  # end of 'postbuilds'
                }],  # mac_breakpad==1
              ],  # conditions
            }],  # OS=mac
            ['OS=="linux" and use_allocator!="none"', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],  # OS=linux
          ],  # end of 'conditions'
        },  # end of target 'remoting_me2me_host'
        {
          'target_name': 'remoting_me2me_native_messaging_host',
          'type': 'executable',
          'product_name': 'native_messaging_host',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            '../base/base.gyp:base',
            'remoting_breakpad',
            'remoting_host',
            'remoting_host_setup_base',
            'remoting_infoplist_strings',
            'remoting_native_messaging_base',
          ],
          'defines': [
            'VERSION=<(version_full)',
          ],
          'sources': [
            'host/setup/me2me_native_messaging_host.cc',
            'host/setup/me2me_native_messaging_host.h',
            'host/setup/me2me_native_messaging_host_entry_point.cc',
            'host/setup/me2me_native_messaging_host_main.cc',
            'host/setup/me2me_native_messaging_host_main.h',
          ],
          'conditions': [
            ['OS=="linux" and use_allocator!="none"', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
            ['OS=="mac"', {
              'mac_bundle': 1,
              'variables': {
                 'host_bundle_id': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_NATIVE_MESSAGING_HOST_BUNDLE_ID@")',
              },
              'xcode_settings': {
                'INFOPLIST_FILE': 'host/setup/native_messaging_host-Info.plist',
                'INFOPLIST_PREPROCESS': 'YES',
                'INFOPLIST_PREPROCESSOR_DEFINITIONS': 'VERSION_FULL="<(version_full)" VERSION_SHORT="<(version_short)" BUNDLE_ID="<(host_bundle_id)"',
              },
              'mac_bundle_resources': [
                'host/setup/native_messaging_host-Info.plist',
                '<!@pymod_do_main(remoting_copy_locales -o -p <(OS) -x <(PRODUCT_DIR) <(remoting_locales))',

                # Localized strings for 'Info.plist'
                '<!@pymod_do_main(remoting_localize --locale_output '
                    '"<(SHARED_INTERMEDIATE_DIR)/remoting/native_messaging_host-InfoPlist.strings/@{json_suffix}.lproj/InfoPlist.strings" '
                    '--print_only <(remoting_locales))',
              ],
              'mac_bundle_resources!': [
                'host/setup/native_messaging_host-Info.plist',
              ],
              'conditions': [
                ['mac_breakpad==1', {
                  'variables': {
                    # A real .dSYM is needed for dump_syms to operate on.
                    'mac_real_dsym': 1,
                  },
                  'copies': [
                    {
                      'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Resources',
                      'files': [
                        '<(PRODUCT_DIR)/crash_inspector',
                        '<(PRODUCT_DIR)/crash_report_sender.app'
                      ],
                    },
                  ],
                  'dependencies': [
                    '../breakpad/breakpad.gyp:dump_syms',
                  ],
                  'postbuilds': [
                    {
                      'postbuild_name': 'Dump Symbols',
                      'variables': {
                        'dump_product_syms_path':
                            'scripts/mac/dump_product_syms',
                      },
                      'action': [
                        '<(dump_product_syms_path)',
                        '<(version_full)',
                      ],
                    },  # end of postbuild 'dump_symbols'
                  ],  # end of 'postbuilds'
                }],  # mac_breakpad==1
              ],  # conditions
            }],  # OS=mac
          ],
        },  # end of target 'remoting_me2me_native_messaging_host'
        {
          'target_name': 'remoting_it2me_native_messaging_host',
          'type': 'executable',
          'product_name': 'remote_assistance_host',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            '../base/base.gyp:base',
            'remoting_base',
            'remoting_breakpad',
            'remoting_host',
            'remoting_it2me_host_static',
            'remoting_native_messaging_base',
            'remoting_protocol',
          ],
          'defines': [
            'VERSION=<(version_full)',
          ],
          'sources': [
            'host/it2me/it2me_native_messaging_host_entry_point.cc',
            'host/it2me/it2me_native_messaging_host_main.cc',
            'host/it2me/it2me_native_messaging_host_main.h',
          ],
          'conditions': [
            ['OS=="linux"', {
              'dependencies': [
                # Always use GTK on Linux, even for Aura builds.
                '../build/linux/system.gyp:gtk',
              ],
            }],
            ['OS=="linux" and use_allocator!="none"', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
            ['OS=="mac"', {
              'mac_bundle': 1,
              'variables': {
                 'host_bundle_id': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_REMOTE_ASSISTANCE_HOST_BUNDLE_ID@")',
              },
              'xcode_settings': {
                'INFOPLIST_FILE': 'host/it2me/remote_assistance_host-Info.plist',
                'INFOPLIST_PREPROCESS': 'YES',
                'INFOPLIST_PREPROCESSOR_DEFINITIONS': 'VERSION_FULL="<(version_full)" VERSION_SHORT="<(version_short)" BUNDLE_ID="<(host_bundle_id)"',
              },
              'mac_bundle_resources': [
                '<(PRODUCT_DIR)/icudtl.dat',
                'host/disconnect_window.xib',
                'host/it2me/remote_assistance_host-Info.plist',
                '<!@pymod_do_main(remoting_copy_locales -o -p <(OS) -x <(PRODUCT_DIR) <(remoting_locales))',

                # Localized strings for 'Info.plist'
                '<!@pymod_do_main(remoting_localize --locale_output '
                    '"<(SHARED_INTERMEDIATE_DIR)/remoting/remote_assistance_host-InfoPlist.strings/@{json_suffix}.lproj/InfoPlist.strings" '
                    '--print_only <(remoting_locales))',
              ],
              'mac_bundle_resources!': [
                'host/it2me/remote_assistance_host-Info.plist',
              ],
              'conditions': [
                ['mac_breakpad==1', {
                  'variables': {
                    # A real .dSYM is needed for dump_syms to operate on.
                    'mac_real_dsym': 1,
                  },
                  'copies': [
                    {
                      'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Resources',
                      'files': [
                        '<(PRODUCT_DIR)/crash_inspector',
                        '<(PRODUCT_DIR)/crash_report_sender.app'
                      ],
                    },
                  ],
                  'dependencies': [
                    '../breakpad/breakpad.gyp:dump_syms',
                  ],
                  'postbuilds': [
                    {
                      'postbuild_name': 'Dump Symbols',
                      'variables': {
                        'dump_product_syms_path':
                            'scripts/mac/dump_product_syms',
                      },
                      'action': [
                        '<(dump_product_syms_path)',
                        '<(version_full)',
                      ],
                    },  # end of postbuild 'dump_symbols'
                  ],  # end of 'postbuilds'
                }],  # mac_breakpad==1
              ],  # conditions
            }],  # OS=mac
          ],
        },  # end of target 'remoting_it2me_native_messaging_host'
      ],  # end of 'targets'
    }],  # OS!="win"

  ],  # end of 'conditions'
}
