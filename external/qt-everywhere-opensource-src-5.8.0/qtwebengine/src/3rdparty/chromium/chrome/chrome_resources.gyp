# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    # Apply Chrome-specific grit settings to all of this file.
    # The advantage is that one entry here applies to the entire file.
    # The caveat is these variables cannot be merged with other variable
    # dictionaries in more inner scopes. If the variable should be merged,
    # consider putting them in a gypi file and including it at the right scope
    # instead. e.g. with chrome_grit_action.gypi.
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/chrome',
    'additional_modules_list_file': '<(SHARED_INTERMEDIATE_DIR)/chrome/browser/internal/additional_modules_list.txt',
  },
  'targets': [
    {
      # GN version: //chrome:extra_resources
      'target_name': 'chrome_extra_resources',
      'type': 'none',
      # These resources end up in resources.pak because they are resources
      # used by internal pages.  Putting them in a separate pak file makes
      # it easier for us to reference them internally.
      'actions': [
        {
          # GN version: //chrome/browser/resources:net_internals_resources
          'action_name': 'generate_net_internals_resources',
          'variables': {
            'grit_grd_file': 'browser/resources/net_internals_resources.grd',
          },
          'includes': [ 'chrome_grit_action.gypi' ],
        },
        {
          # GN version: //chrome/browser/resources:invalidations_resources
          'action_name': 'generate_invalidations_resources',
          'variables': {
            'grit_grd_file': 'browser/resources/invalidations_resources.grd',
            },
          'includes': ['chrome_grit_action.gypi' ],
        },
        {
          # GN version: //chrome/browser/resources:password_manager_internals_resources
          'action_name': 'generate_password_manager_internals_resources',
          'variables': {
            'grit_grd_file': 'browser/resources/password_manager_internals_resources.grd',
          },
          'includes': [ 'chrome_grit_action.gypi' ],
        },
        {
          # GN version: //chrome/browser/resources:policy_resources
          'action_name': 'generate_policy_resources',
          'variables': {
            'grit_grd_file': 'browser/resources/md_policy/policy_resources.grd',
          },
          'includes': [ 'chrome_grit_action.gypi' ],
        },
        {
          # GN version: //chrome/browser/resources:translate_internals_resources
          'action_name': 'generate_translate_internals_resources',
          'variables': {
            'grit_grd_file': 'browser/resources/translate_internals_resources.grd',
          },
          'includes': [ 'chrome_grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
      'conditions': [
        ['OS != "ios"', {
          'dependencies': [
            '../components/components_resources.gyp:components_resources',
            '../content/browser/devtools/devtools_resources.gyp:devtools_resources',
            '../content/browser/tracing/tracing_resources.gyp:tracing_resources',
          ],
          'actions': [
            {
              # GN version: //chrome/browser/resources:component_extension_resources
              'action_name': 'generate_component_extension_resources',
              'variables': {
                'grit_grd_file': 'browser/resources/component_extension_resources.grd',
              },
              'includes': [ 'chrome_grit_action.gypi' ],
            },
            {
              # GN version: //chrome/browser/resources:options_resources
              'action_name': 'generate_options_resources',
              'variables': {
                'grit_grd_file': 'browser/resources/options_resources.grd',
              },
              'includes': [ 'chrome_grit_action.gypi' ],
            },
            {
              # GN version: //chrome/browser/resources:settings_resources
              'action_name': 'generate_settings_resources',
              'variables': {
                'grit_grd_file': 'browser/resources/settings/settings_resources.grd',
              },
              'includes': [ 'chrome_grit_action.gypi' ],
            },
          ],
          'copies': [
            {
              # GN version: //chrome/browser/resources:extension_resource_demo
              'destination': '<(PRODUCT_DIR)/resources/extension/demo',
              'files': [
                'browser/resources/extension_resource/demo/library.js',
              ],
            },
          ],
        }],
        ['chromeos==1 and disable_nacl==0 and disable_nacl_untrusted==0', {
          'dependencies': [
            'browser/resources/chromeos/chromevox/chromevox.gyp:chromevox',
          ],
        }],
        ['enable_extensions==1', {
          'actions': [
            {
              # GN version: //chrome/browser/resources:quota_internals_resources
              'action_name': 'generate_quota_internals_resources',
              'variables': {
                'grit_grd_file': 'browser/resources/quota_internals_resources.grd',
              },
              'includes': [ 'chrome_grit_action.gypi' ],
            },
            {
              # GN version: //chrome/browser/resources:sync_file_system_internals_resources
              'action_name': 'generate_sync_file_system_internals_resources',
              'variables': {
                'grit_grd_file': 'browser/resources/sync_file_system_internals_resources.grd',
              },
              'includes': [ 'chrome_grit_action.gypi' ],
            },
          ],
        }],
      ],
    },
    {
      # GN version: //chrome/browser:chrome_internal_resources_gen
      'target_name': 'chrome_internal_resources_gen',
      'type': 'none',
      'conditions': [
        ['branding=="Chrome"', {
          'actions': [
            {
              'action_name': 'generate_transform_additional_modules_list',
              'variables': {
                'additional_modules_input_path':
                  'browser/internal/resources/additional_modules_list.input',
                'additional_modules_py_path':
                  'browser/internal/transform_additional_modules_list.py',
              },
              'inputs': [
                '<(additional_modules_input_path)',
              ],
              'outputs': [
                '<(additional_modules_list_file)',
              ],
              'action': [
                'python',
                '<(additional_modules_py_path)',
                '<(additional_modules_input_path)',
                '<@(_outputs)',
              ],
              'message': 'Transforming additional modules list',
            }
          ],
        }],
      ],
    },
    {
      # TODO(mark): It would be better if each static library that needed
      # to run grit would list its own .grd files, but unfortunately some
      # of the static libraries currently have circular dependencies among
      # generated headers.
      #
      # GN version: //chrome:resources
      'target_name': 'chrome_resources',
      'type': 'none',
      'dependencies': [
        'chrome_internal_resources_gen',
        'chrome_web_ui_mojo_bindings.gyp:web_ui_mojo_bindings',
        '../url/url.gyp:url_interfaces_mojom',
      ],
      'actions': [
        {
          # GN version: //chrome/browser:resources
          'action_name': 'generate_browser_resources',
          'variables': {
            'grit_grd_file': 'browser/browser_resources.grd',
            'grit_additional_defines': [
              '-E', 'additional_modules_list_file=<(additional_modules_list_file)',
              '-E', 'root_gen_dir=<(SHARED_INTERMEDIATE_DIR)',
            ],
          },
          'includes': [ 'chrome_grit_action.gypi' ],
        },
        {
          # GN version: //chrome/common:resources
          'action_name': 'generate_common_resources',
          'variables': {
            'grit_grd_file': 'common/common_resources.grd',
          },
          'includes': [ 'chrome_grit_action.gypi' ],
        },
        {
          # GN version: //chrome/renderer:resources
          'action_name': 'generate_renderer_resources',
          'variables': {
            'grit_grd_file': 'renderer/resources/renderer_resources.grd',
          },
          'includes': [ 'chrome_grit_action.gypi' ],
        },
      ],
      'conditions': [
        ['enable_extensions==1', {
          'actions': [
            {
              # GN version: //chrome/common:extensions_api_resources
              'action_name': 'generate_extensions_api_resources',
              'variables': {
                'grit_grd_file': 'common/extensions_api_resources.grd',
              },
              'includes': [ 'chrome_grit_action.gypi' ],
            }
          ],
        }],
        ['OS != "android" and OS != "ios"', {
          'dependencies': [ 'make_file_types_protobuf' ]
        }],
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      # TODO(mark): It would be better if each static library that needed
      # to run grit would list its own .grd files, but unfortunately some
      # of the static libraries currently have circular dependencies among
      # generated headers.
      #
      # GN version: //chrome:strings
      'target_name': 'chrome_strings',
      'type': 'none',
      'actions': [
        {
          # GN version: //chrome/app/resources:locale_settings
          'action_name': 'generate_locale_settings',
          'variables': {
            'grit_grd_file': 'app/resources/locale_settings.grd',
          },
          'includes': [ 'chrome_grit_action.gypi' ],
        },
        {
          # GN version: //chrome/app:chromium_strings
          'action_name': 'generate_chromium_strings',
          'variables': {
            'grit_grd_file': 'app/chromium_strings.grd',
          },
          'includes': [ 'chrome_grit_action.gypi' ],
        },
        {
          # GN version: //chrome/app:generated_resources
          'action_name': 'generate_generated_resources',
          'variables': {
            'grit_grd_file': 'app/generated_resources.grd',
          },
          'includes': [ 'chrome_grit_action.gypi' ],
        },
        {
          # GN version: //chrome/app:google_chrome_strings
          'action_name': 'generate_google_chrome_strings',
          'variables': {
            'grit_grd_file': 'app/google_chrome_strings.grd',
          },
          'includes': [ 'chrome_grit_action.gypi' ],
        },
        {
          # GN version: //chrome/app:settings_strings
          'action_name': 'generate_settings_strings',
          'variables': {
            'grit_grd_file': 'app/settings_strings.grd',
          },
          'includes': [ 'chrome_grit_action.gypi' ],
        },
        {
          # GN version: //chrome/app:settings_chromium_strings
          'action_name': 'generate_settings_chromium_strings',
          'variables': {
            'grit_grd_file': 'app/settings_chromium_strings.grd',
          },
          'includes': [ 'chrome_grit_action.gypi' ],
        },
        {
          # GN version: //chrome/app:settings_google_chrome_strings
          'action_name': 'generate_settings_google_chrome_strings',
          'variables': {
            'grit_grd_file': 'app/settings_google_chrome_strings.grd',
          },
          'includes': [ 'chrome_grit_action.gypi' ],
        },
      ],
    },

    {
      # GN version: //chrome/browser/resources/safe_browsing:make_file_types_protobuf
      # Convert the ascii proto file to a binary resource.
      'target_name': 'make_file_types_protobuf',
      'type': 'none',
      'hard_dependency': 1,
      'dependencies': [
        'chrome.gyp:safe_browsing_proto',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:py_proto',
      ],
      'actions': [
        {
          'action_name': 'generate_file_types_protobuf',
          'variables' : {
            'script_file':'browser/resources/safe_browsing/gen_file_type_proto.py',
            'asciipb_file' : 'browser/resources/safe_browsing/download_file_types.asciipb',
            'output_dir' : '<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/safe_browsing',
            'output_basename' : 'download_file_types.pb',
            'conditions': [
              ['OS=="android"', {
                'platform': 'android'
              }, 'chromeos==1', {
                'platform': 'chromeos'
              }, 'OS=="linux"', {
                'platform': 'linux'
              }, 'OS=="mac"', {
                'platform': 'mac'
              }, 'OS=="win"', {
                'platform': 'win'
              }, {
                # This will cause the script to fail
                'platform': 'unknown_target_arch'
              }],
            ],
          },
          'inputs': [
            '<(script_file)',
            '<(asciipb_file)',
          ],
          'outputs': [
            '<(output_dir)/<(output_basename)',
          ],
          'action': [
            'python',
            '<(script_file)',
            '-w',
            '-i', '<(asciipb_file)',
            '-d', '<(output_dir)',
            '-o', '<(output_basename)',
            '-t', '<(platform)',
            '-p', '<(PRODUCT_DIR)/pyproto',
            '-p', '<(PRODUCT_DIR)/pyproto/chrome/common/safe_browsing',
          ],
          'message': 'Generating download_file_types.pb.',
        }
      ],
    },
    {
      # GN version: //chrome/browser/metrics/variations:chrome_ui_string_overrider_factory_gen_sources
      'target_name': 'make_chrome_ui_string_overrider_factory',
      'type': 'none',
      'hard_dependency': 1,
      'dependencies': [
        'chrome_strings',
        '../components/components_strings.gyp:components_strings',
      ],
      'actions': [
        {
          'action_name': 'generate_ui_string_overrider',
          'inputs': [
            '../components/variations/service/generate_ui_string_overrider.py',
            '<(grit_out_dir)/grit/chromium_strings.h',
	    '<(grit_out_dir)/grit/generated_resources.h',
            '<(grit_out_dir)/grit/google_chrome_strings.h',
            '<(grit_out_dir)/grit/settings_chromium_strings.h',
            '<(grit_out_dir)/grit/settings_google_chrome_strings.h',
            '<(grit_out_dir)/grit/settings_strings.h',
            '<(SHARED_INTERMEDIATE_DIR)/components/strings/grit/components_chromium_strings.h',
            '<(SHARED_INTERMEDIATE_DIR)/components/strings/grit/components_google_chrome_strings.h',
	    '<(SHARED_INTERMEDIATE_DIR)/components/strings/grit/components_strings.h'
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser/metrics/variations/ui_string_overrider_factory.cc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser/metrics/variations/ui_string_overrider_factory.h',
          ],
          'action': [
            'python',
            '../components/variations/service/generate_ui_string_overrider.py',
            '-N', 'chrome_variations',
            '-o', '<(SHARED_INTERMEDIATE_DIR)',
            '-S', 'chrome/browser/metrics/variations/ui_string_overrider_factory.cc',
            '-H', 'chrome/browser/metrics/variations/ui_string_overrider_factory.h',
            '<(grit_out_dir)/grit/chromium_strings.h',
	    '<(grit_out_dir)/grit/generated_resources.h',
            '<(grit_out_dir)/grit/google_chrome_strings.h',
            '<(grit_out_dir)/grit/settings_chromium_strings.h',
            '<(grit_out_dir)/grit/settings_google_chrome_strings.h',
            '<(grit_out_dir)/grit/settings_strings.h',
            '<(SHARED_INTERMEDIATE_DIR)/components/strings/grit/components_chromium_strings.h',
            '<(SHARED_INTERMEDIATE_DIR)/components/strings/grit/components_google_chrome_strings.h',
	    '<(SHARED_INTERMEDIATE_DIR)/components/strings/grit/components_strings.h'
          ],
          'message': 'Generating generated resources map.',
        }
      ],
    },
    {
      # GN version: //chrome/browser/metrics/variations:chrome_ui_string_overrider_factory
      'target_name': 'chrome_ui_string_overrider_factory',
      'type': 'static_library',
      'dependencies': [
        '../components/components.gyp:variations_service',
        'make_chrome_ui_string_overrider_factory',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/chrome/browser/metrics/variations/ui_string_overrider_factory.cc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/browser/metrics/variations/ui_string_overrider_factory.h',
      ],
    },
    {
      # GN version: //chrome/app/resources:platform_locale_settings
      'target_name': 'platform_locale_settings',
      'type': 'none',
      'variables': {
        'conditions': [
          ['OS=="win"', {
            'platform_locale_settings_grd':
                'app/resources/locale_settings_win.grd',
          },],
          ['OS=="linux"', {
            'conditions': [
              ['chromeos==1', {
                'platform_locale_settings_grd':
                    'app/resources/locale_settings_<(branding_path_component)os.grd',
              }, {  # chromeos==0
                'platform_locale_settings_grd':
                    'app/resources/locale_settings_linux.grd',
              }],
            ],
          },],
          ['os_posix == 1 and OS != "mac" and OS != "ios" and OS != "linux"', {
            'platform_locale_settings_grd':
                'app/resources/locale_settings_linux.grd',
          },],
          ['OS == "mac" or OS == "ios"', {
            'platform_locale_settings_grd':
                'app/resources/locale_settings_mac.grd',
          }],
        ],  # conditions
      },  # variables
      'actions': [
        {
          'action_name': 'generate_platform_locale_settings',
          'variables': {
            'grit_grd_file': '<(platform_locale_settings_grd)',
          },
          'includes': [ 'chrome_grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      # GN version: //chrome/app/theme:theme_resources
      'target_name': 'theme_resources',
      'type': 'none',
      'dependencies': [
        '../ui/resources/ui_resources.gyp:ui_resources',
        'chrome_unscaled_resources',
      ],
      'actions': [
        {
          'action_name': 'generate_theme_resources',
          'variables': {
            'grit_grd_file': 'app/theme/theme_resources.grd',
          },
          'includes': [ 'chrome_grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      # GN version: //chrome:packed_extra_resources
      'target_name': 'packed_extra_resources',
      'type': 'none',
      'dependencies': [
        'chrome_extra_resources',
        'packed_resources',
      ],
      'actions': [
        {
          'includes': ['chrome_repack_resources.gypi']
        },
      ],
      'conditions': [
        ['OS != "mac" and OS != "ios"', {
          # We'll install the resource files to the product directory.  The Mac
          # copies the results over as bundle resources in its own special way.
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)',
              'files': [
                '<(SHARED_INTERMEDIATE_DIR)/repack/resources.pak'
              ],
            },
          ],
        }],
      ],
    },
    {
      # GN version: //chrome:packed_resources
      'target_name': 'packed_resources',
      'type': 'none',
      'dependencies': [  # Update duplicate logic in repack_locales.py
        # MSVS needs the dependencies explictly named, Make is able to
        # derive the dependencies from the output files.
        'chrome_resources',
        'chrome_strings',
        'platform_locale_settings',
        'theme_resources',
        '<(DEPTH)/components/components_strings.gyp:components_strings',
        '<(DEPTH)/net/net.gyp:net_resources',
        '<(DEPTH)/ui/resources/ui_resources.gyp:ui_resources',
        '<(DEPTH)/ui/strings/ui_strings.gyp:ui_strings',
      ],
      'actions': [
        {
          # GN version: //chrome:repack_locales_pack
          'action_name': 'repack_locales_pack',
          'variables': {
            'pak_locales': '<(locales)',
          },
          'includes': ['chrome_repack_locales.gypi']
        },
        {
          # GN version: //chrome:repack_pseudo_locales_pack
          'action_name': 'repack_pseudo_locales_pack',
          'variables': {
            'pak_locales': '<(pseudo_locales)',
          },
          'includes': ['chrome_repack_locales.gypi']
        },
        {
          'includes': ['chrome_repack_chrome_100_percent.gypi']
        },
        {
          'includes': ['chrome_repack_chrome_200_percent.gypi']
        },
      ],
      'conditions': [  # GN version: chrome_repack_locales.gni template("_repack_one_locale")
        ['OS != "ios"', {
          'dependencies': [  # Update duplicate logic in repack_locales.py
            '<(DEPTH)/content/app/resources/content_resources.gyp:content_resources',
            '<(DEPTH)/content/app/strings/content_strings.gyp:content_strings',
            '<(DEPTH)/device/bluetooth/bluetooth_strings.gyp:bluetooth_strings',
            '<(DEPTH)/third_party/WebKit/public/blink_resources.gyp:blink_resources',
          ],
        }, {  # else
          'dependencies': [  # Update duplicate logic in repack_locales.py
            '<(DEPTH)/ios/chrome/ios_chrome_resources.gyp:ios_strings_gen',
          ],
          'actions': [
            {
              'includes': ['chrome_repack_chrome_300_percent.gypi']
            },
          ],
        }],
        ['use_ash==1', {
          'dependencies': [  # Update duplicate logic in repack_locales.py
             '<(DEPTH)/ash/ash_resources.gyp:ash_resources',
             '<(DEPTH)/ash/ash_strings.gyp:ash_strings',
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
             '<(DEPTH)/ui/views/resources/views_resources.gyp:views_resources',
          ],
        }],
        ['chromeos==1', {
          'dependencies': [  # Update duplicate logic in repack_locales.py
            '<(DEPTH)/remoting/remoting.gyp:remoting_resources',
            '<(DEPTH)/ui/chromeos/ui_chromeos.gyp:ui_chromeos_resources',
            '<(DEPTH)/ui/chromeos/ui_chromeos.gyp:ui_chromeos_strings',
          ],
        }],
        ['OS != "android" and OS != "ios"', {
          'dependencies': [  # Update duplicate logic in repack_locales.py
            '<(DEPTH)/third_party/libaddressinput/libaddressinput.gyp:libaddressinput_strings',
          ],
        }],
        ['enable_extensions==1', {
          'dependencies': [  # Update duplicate logic in repack_locales.py
            '<(DEPTH)/extensions/extensions_strings.gyp:extensions_strings',
          ],
        }],
        ['enable_app_list==1', {
          'dependencies': [
             '<(DEPTH)/ui/app_list/resources/app_list_resources.gyp:app_list_resources',
          ],
        }],
        ['OS == "mac"', {
          'actions': [
            {
              'includes': ['chrome_repack_chrome_material_100_percent.gypi']
            },
            {
              'includes': ['chrome_repack_chrome_material_200_percent.gypi']
            },
          ],
        }],
        ['OS != "mac" and OS != "ios"', {
          # Copy pak files to the product directory. These files will be picked
          # up by the following installer scripts:
          #   - Windows: chrome/installer/mini_installer/chrome.release
          #   - Linux: chrome/installer/linux/internal/common/installer.include
          # Ensure that the above scripts are updated when adding or removing
          # pak files.
          # Copying files to the product directory is not needed on the Mac
          # since the framework build phase will copy them into the framework
          # bundle directly.
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)',
              'files': [
                '<(SHARED_INTERMEDIATE_DIR)/repack/chrome_100_percent.pak'
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/locales',
              'files': [
                '<!@pymod_do_main(repack_locales -o -p <(OS) -g <(grit_out_dir) -s <(SHARED_INTERMEDIATE_DIR) -x <(SHARED_INTERMEDIATE_DIR) <(locales))'
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/pseudo_locales',
              'files': [
                '<!@pymod_do_main(repack_locales -o -p <(OS) -g <(grit_out_dir) -s <(SHARED_INTERMEDIATE_DIR) -x <(SHARED_INTERMEDIATE_DIR) <(pseudo_locales))'
              ],
            },
          ],
          'conditions': [
            ['branding=="Chrome"', {
              'copies': [
                {
                  # This location is for the Windows and Linux builds. For
                  # Windows, the chrome.release file ensures that these files
                  # are copied into the installer. Note that we have a separate
                  # section in chrome_dll.gyp to copy these files for Mac, as it
                  # needs to be dropped inside the framework.
                  'destination': '<(PRODUCT_DIR)/default_apps',
                  'files': ['<@(default_apps_list)']
                },
              ],
            }],
            ['enable_hidpi == 1', {
              'copies': [
                {
                  'destination': '<(PRODUCT_DIR)',
                  'files': [
                    '<(SHARED_INTERMEDIATE_DIR)/repack/chrome_200_percent.pak',
                  ],
                },
              ],
            }],
          ], # conditions
        }], # end OS != "mac" and OS != "ios"
      ], # conditions
    },
    {
      # GN version: //chrome/app/theme:chrome_unscaled_resources
      'target_name': 'chrome_unscaled_resources',
      'type': 'none',
      'actions': [
        {
          'action_name': 'generate_chrome_unscaled_resources',
          'variables': {
            'grit_grd_file': 'app/theme/chrome_unscaled_resources.grd',
          },
          'includes': [ 'chrome_grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      # GN version: //chrome/browser/resources:options_test_resources
      'target_name': 'options_test_resources',
      'type': 'none',
      'actions': [
        {
          'action_name': 'generate_options_test_resources',
          'variables': {
            'grit_grd_file': 'browser/resources/options_test_resources.grd',
          },
          'includes': [ 'chrome_grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      # GN version: //chrome/test/data/resources:webui_test_resources
      'target_name': 'webui_test_resources',
      'type': 'none',
      'actions': [
        {
          'action_name': 'generate_webui_test_resources',
          'variables': {
            'grit_grd_file': 'test/data/webui_test_resources.grd',
          },
          'includes': [ 'chrome_grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      # GN version: //chrome:browser_tests_pak
      'target_name': 'browser_tests_pak',
      'type': 'none',
      'dependencies': [
        'options_test_resources',
        'webui_test_resources',
      ],
      'actions': [
        {
          'action_name': 'repack_browser_tests_pak',
          'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/chrome/options_test_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/chrome/webui_test_resources.pak',
            ],
            'pak_output': '<(PRODUCT_DIR)/browser_tests.pak',
          },
          'includes': [ '../build/repack_action.gypi' ],
        },
      ],
    },
  ], # targets
}
