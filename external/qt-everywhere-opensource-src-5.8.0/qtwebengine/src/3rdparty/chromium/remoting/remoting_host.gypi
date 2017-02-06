# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'remoting_enable.gypi',
  ],

  'conditions': [
    ['OS=="mac"', {
      'includes': [
        'remoting_host_mac.gypi',
      ],
    }],
    ['OS=="win"', {
      'includes': [
        'remoting_host_win.gypi',
      ],
    }],
    ['OS=="linux"', {
      'includes': [
        'remoting_host_linux.gypi',
      ],
    }],

    ['enable_remoting_host==1', {
      'targets': [
        {
          # GN version: //remoting/host
          'target_name': 'remoting_host',
          'type': 'static_library',
          'variables': {
            'enable_wexit_time_destructors': 1,
            'host_bundle_name': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_HOST_BUNDLE_NAME@")',
            'prefpane_bundle_name': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_PREFPANE_BUNDLE_NAME@")',
          },
          'dependencies': [
            'remoting_base',
            'remoting_host_credits',
            'remoting_protocol',
            'remoting_resources',
            '../base/base.gyp:base_i18n',
            '../components/components.gyp:policy',
            '../components/components.gyp:policy_component_common',
            '../crypto/crypto.gyp:crypto',
            '../google_apis/google_apis.gyp:google_apis',
            '../ipc/ipc.gyp:ipc',
            '../ui/events/events.gyp:dom_keycode_converter',
          ],
          'defines': [
            'HOST_BUNDLE_NAME="<(host_bundle_name)"',
            'PREFPANE_BUNDLE_NAME="<(prefpane_bundle_name)"',
            'VERSION=<(version_full)',
          ],
          'sources': [
            '<@(remoting_host_sources)',
          ],
          'conditions': [
            ['OS=="linux"', {
              'conditions': [
                ['use_x11==1', {
                  'dependencies': [
                    '../build/linux/system.gyp:x11',
                    '../build/linux/system.gyp:xext',
                    '../build/linux/system.gyp:xfixes',
                    '../build/linux/system.gyp:xi',
                    '../build/linux/system.gyp:xrandr',
                    '../build/linux/system.gyp:xtst',
                  ],
                }, {
                'sources!' : [
                  'host/linux/unicode_to_keysym.cc',
                ],
                }],
                ['chromeos==0 and use_ozone==0', {
                  'dependencies': [
                    # use GTK on Linux, even for Aura builds.
                    '../build/linux/system.gyp:gtk2',
                  ],
                }]
              ],
            }],
            ['chromeos==1', {
              'dependencies' : [
                '../cc/cc.gyp:cc',
                '../gpu/gpu.gyp:command_buffer_common',
                '../ppapi/ppapi_internal.gyp:ppapi_host',
                '../skia/skia.gyp:skia',
                '../ui/aura/aura.gyp:aura',
                '../ui/compositor/compositor.gyp:compositor',
                '../ui/events/events.gyp:events',
                '../ui/events/platform/events_platform.gyp:events_platform',
                '../ui/views/views.gyp:views',
              ],
              'include_dirs': [
                '../third_party/skia/include/utils',
              ],
              'sources!' : [
                'host/clipboard_x11.cc',
                'host/continue_window_linux.cc',
                'host/curtain_mode_linux.cc',
                'host/disconnect_window_linux.cc',
                'host/linux/x_server_clipboard.cc',
                'host/linux/x_server_clipboard.h',
                'host/local_input_monitor_x11.cc',
              ],
              'conditions': [
                ['use_ash==1', {
                  'dependencies': [
                    '../ash/ash.gyp:ash',
                  ],
                }],
                ['use_ozone==0', {
                  'sources!': [
                    'host/input_injector_chromeos.cc',
                    'host/input_injector_chromeos.h',
                  ],
                }, {  # use_ozone==1
                  'dependencies' : [
                    '../ui/ozone/ozone.gyp:ozone',
                  ]
                }],
              ],
            }],
            ['OS=="mac"', {
              'dependencies': [
                '../third_party/google_toolbox_for_mac/google_toolbox_for_mac.gyp:google_toolbox_for_mac',
              ],
              'link_settings': {
                'libraries': [
                  '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
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
            ['enable_webrtc==1', {
              'dependencies': [
                '../third_party/webrtc/modules/modules.gyp:desktop_capture',
              ],
            }],
            ['remoting_use_gcd==1', {
              'defines': [
                'USE_GCD',
              ]
            }],
          ],
        },  # end of target 'remoting_host'

        {
          # GN version: //remoting/host:credits
          'target_name': 'remoting_host_credits',
          'type': 'none',
          'actions': [
            {
              'action_name': 'Build remoting host credits',
              'inputs': [
                '../tools/licenses.py',
                'host/installer/credits.tmpl',
                'host/installer/credits_entry.tmpl',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/remoting/CREDITS.txt',
              ],
              'hard_dependency': 1,
              'action': ['python',
                         '../tools/licenses.py',
                         'credits',
                         '<(SHARED_INTERMEDIATE_DIR)/remoting/CREDITS.txt',
                         '--file-template', 'host/installer/credits.tmpl',
                         '--entry-template', 'host/installer/credits_entry.tmpl',
              ],
            },
          ],
        },

        {
          # GN version: //remoting/host/native_messaging
          'target_name': 'remoting_native_messaging_base',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            '../base/base.gyp:base',
          ],
          'sources': [
            '<@(remoting_host_native_messaging_sources)',
          ],
        },  # end of target 'remoting_native_messaging_base'

        {
          # GN version: //remoting/host/setup
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
            '<@(remoting_host_setup_sources)',
          ],
          'conditions': [
            ['OS=="win"', {
              'dependencies': [
                'remoting_lib_idl',
              ],
              # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
              'msvs_disabled_warnings': [4267, ],
            }],
          ],
        },  # end of target 'remoting_host_setup_base'

        # Generates native messaging manifest files.
        {
          # GN: //remoting/host:remoting_native_messaging_manifests
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
          # GN: //remoting/host:remoting_infoplist_strings
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
        {
          # GN version: //remoting/host/it2me:common
          'target_name': 'remoting_it2me_host_static',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            '../base/base.gyp:base_i18n',
            '../components/components.gyp:policy',
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
            '<@(remoting_it2me_host_static_sources)',
          ],
        },  # end of target 'remoting_it2me_host_static'
      ],  # end of 'targets'
    }],  # 'enable_remoting_host==1'

    ['enable_me2me_host==1', {
      'targets': [
        {
          'target_name': 'remoting_me2me_host_static',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:base_i18n',
            '../components/components.gyp:policy',
            '../components/components.gyp:policy_component_common',
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
            'host/pam_authorization_factory_posix.cc',
            'host/pam_authorization_factory_posix.h',
            'host/remoting_me2me_host.cc',
          ],
          'conditions': [
            ['OS=="linux"', {
              'link_settings': {
                'libraries': [
                  '-lpam',
                ],
              },
            }],
            ['OS=="mac"', {
              'link_settings': {
                'libraries': [
                  'libpam.a',
               ],
              },
            }],
            ['OS=="mac" and buildtype=="Official"', {
              'sources': [
                'internal/internal_mac-inl.h'
              ],
              'defines': [
                'USE_REMOTING_MACOSX_INTERNAL'
              ],
            }],
            ['remoting_use_gcd==1', {
              'defines': [
                'USE_GCD',
              ]
            }],
          ],  # end of 'conditions'
        },  # end of target 'remoting_me2me_host_static'
      ]  # end of targets
    }],  # end of enable_me2me_host==1

    ['OS!="win" and enable_me2me_host==1', {
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
                '<(SHARED_INTERMEDIATE_DIR)/remoting/CREDITS.txt',
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
          ],  # end of 'conditions'
        },  # end of target 'remoting_me2me_host'
        {
          # GN: //remoting/host:native_messaging_host
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
            'host/setup/me2me_native_messaging_host_entry_point.cc',
            'host/setup/me2me_native_messaging_host_main.cc',
            'host/setup/me2me_native_messaging_host_main.h',
          ],
          'conditions': [
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
      ], # targets
    }], # end of OS!="win" and enable_me2me_host==1

    ['OS!="win" and enable_remoting_host==1', {
      'targets': [
        {
          # GN target: //remoting/host:remoting_start_host
          'target_name': 'remoting_start_host',
          'type': 'executable',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            'remoting_host_setup_base',
          ],
          'sources': [
            'host/setup/host_starter.cc',
            'host/setup/host_starter.h',
            'host/setup/start_host_entry_point.cc',
            'host/setup/start_host_main.cc',
            'host/setup/start_host_main.h',
          ],
        },  # end of target 'remoting_start_host'
      ],
      'conditions': [
        ['chromeos==0', {
          'targets': [
            {
              # GN: //remoting/host/it2me:remote_assistance_host
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
                ['OS=="linux" and chromeos==0 and use_ozone==0', {
                  'dependencies': [
                    # Always use GTK on Linux, even for Aura builds.
                    '../build/linux/system.gyp:gtk2',
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
              ],  # end of conditions
            },  # end of target 'remoting_it2me_native_messaging_host'
          ],  # targets
        }, {  # chromeos==0
          'targets': [
            {
              # Dummy target for chromeos==1
              'target_name': 'remoting_it2me_native_messaging_host',
              'type': 'executable',
            },
          ],  # targets
        }],  # end of chromeos==0
      ],  # end of conditions
    }],  # end of OS!="win" and enable_remoting_host==1
  ],  # end of 'conditions'
}
