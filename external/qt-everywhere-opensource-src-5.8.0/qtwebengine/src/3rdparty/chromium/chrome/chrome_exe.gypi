# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //chrome
      'target_name': 'chrome',
      'type': 'none',
      'dependencies': [ 'chrome_initial', ],
      'conditions': [
        ['OS == "win"', {
          'actions': [
            {
              'variables': {
                'reorder_py_path': '<(DEPTH)/build/win/reorder-imports.py',
                # See comment in chrome_dll.gypi in the hardlink_to_output
                # target for why this cannot be 'initial' like the DLL.
                'exe_input_path':'$(OutDir)\\initialexe',
                'exe_output_path':'<(PRODUCT_DIR)',
              },
              'action_name': 'reorder_imports',
              'inputs': [
                '<(reorder_py_path)',
                '$(OutDir)\\initialexe\\chrome.exe',
              ],
              'outputs': [
                '<(PRODUCT_DIR)\\chrome.exe',
                '<(PRODUCT_DIR)\\chrome.exe.pdb',
              ],
              'action': [
                'python',
                '<(reorder_py_path)',
                '-i', '<(exe_input_path)',
                '-o', '<(exe_output_path)',
                '-a', '<(target_arch)',
              ],
              'message': 'Reordering Imports',
            },
          ],
        }],
      ],
    },
    {
      # GN version: //chrome:chrome_initial
      'target_name': 'chrome_initial',
      'type': 'executable',
      'dependencies' : [
        '../chrome/common_constants.gyp:version_header',
        '../chrome/chrome_features.gyp:chrome_common_features',
        '../third_party/kasko/kasko.gyp:kasko_features',
      ],
      # Name the exe chrome.exe, not chrome_initial.exe.
      'product_name': 'chrome',
      'mac_bundle': 1,
      'variables': {
        'use_system_xdg_utils%': 0,
        'enable_wexit_time_destructors': 1,
      },
      'sources': [
        # Note that due to InitializeSandboxInfo, this must be directly linked
        # into chrome.exe, not into a dependent.
        '<(DEPTH)/content/app/sandbox_helper_win.cc',
        '<(DEPTH)/content/public/common/content_switches.cc',
        'app/chrome_exe_load_config_win.cc',
        'app/chrome_exe_main_aura.cc',
        'app/chrome_exe_main_mac.c',
        'app/chrome_exe_main_win.cc',
        'app/chrome_exe_resource.h',
        'app/chrome_watcher_client_win.cc',
        'app/chrome_watcher_client_win.h',
        'app/chrome_watcher_command_line_win.cc',
        'app/chrome_watcher_command_line_win.h',
        'app/kasko_client.cc',
        'app/kasko_client.h',
        'app/main_dll_loader_win.cc',
        'app/main_dll_loader_win.h',
      ],
      'mac_bundle_resources': [
        'app/app-Info.plist',
      ],
      # TODO(mark): Come up with a fancier way to do this.  It should only
      # be necessary to list app-Info.plist once, not the three times it is
      # listed here.
      'mac_bundle_resources!': [
        'app/app-Info.plist',
      ],
      'xcode_settings': {
        'CHROMIUM_STRIP_SAVE_FILE': 'app/app.saves',
        'INFOPLIST_FILE': 'app/app-Info.plist',
      },
      'conditions': [
        ['order_text_section!=""', {
          'target_conditions' : [
            ['_toolset=="target"', {
              'ldflags': [
                '-Wl,-section-ordering-file=<(order_text_section)' ],
            }],
          ]
        }],
        ['OS == "android"', {
          # Don't put the 'chrome' target in 'all' on android
          'suppress_wildcard': 1,
        }],
        ['os_posix == 1 and OS != "mac" and OS != "android"', {
          'actions': [
            {
              'action_name': 'manpage',
              'conditions': [
                [ 'branding == "Chrome"', {
                  'variables': {
                    'name': 'Google Chrome',
                    'filename': 'google-chrome',
                    'confdir': 'google-chrome',
                  },
                }, { # else branding!="Chrome"
                  'variables': {
                    'name': 'Chromium',
                    'filename': 'chromium-browser',
                    'confdir': 'chromium',
                  },
                }],
              ],
              'inputs': [
                'tools/build/linux/sed.py',
                'app/resources/manpage.1.in',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/chrome.1',
              ],
              'action': [
                'python',
                'tools/build/linux/sed.py',
                'app/resources/manpage.1.in',
                '<@(_outputs)',
                '-e', 's/@@NAME@@/<(name)/',
                '-e', 's/@@FILENAME@@/<(filename)/',
                '-e', 's/@@CONFDIR@@/<(confdir)/',
              ],
              'message': 'Generating manpage'
            },
          ],
          'conditions': [
            ['profiling==0 and linux_disable_pie==0', {
              'ldflags': [
                '-pie',
              ],
            }],
            ['use_system_xdg_utils==0', {
              'copies': [
                {
                  'destination': '<(PRODUCT_DIR)',
                  'files': ['tools/build/linux/chrome-wrapper',
                            '../third_party/xdg-utils/scripts/xdg-mime',
                            '../third_party/xdg-utils/scripts/xdg-settings',
                            'app/theme/<(branding_path_component)/product_logo_48.png',
                            ],
                },
              ],
            }],
            # x11 build. Needed for chrome_main.cc initialization of libraries.
            ['use_x11==1', {
              'dependencies': [
                '../build/linux/system.gyp:x11',
                '../build/linux/system.gyp:xext',
              ],
            }],
            ['OS=="linux" and enable_plugins==1', {
              'dependencies': [
                '../pdf/pdf.gyp:pdf',
              ],
            }],
          ],
          'sources': [
            'app/chrome_dll_resource.h',
            'app/chrome_main.cc',
            'app/chrome_main_delegate.cc',
            'app/chrome_main_delegate.h',
          ],
          'dependencies': [
            # On Linux, link the dependencies (libraries) that make up actual
            # Chromium functionality directly into the executable.
            '<@(chromium_browser_dependencies)',
            '<@(chromium_child_dependencies)',
            '../content/content.gyp:content_app_both',
            # Needed for chrome_main.cc initialization of libraries.
            '../build/linux/system.gyp:pangocairo',
            'chrome_features.gyp:chrome_common_features',
            # Needed to use the master_preferences functions
            'installer_util',
          ],
        }],
        ['OS=="mac"', {
          'conditions': [
            ['mac_breakpad==1', {
              'variables': {
                # A real .dSYM is needed for dump_syms to operate on.
                'mac_real_dsym': 1,
              },
              'xcode_settings': {
                # With mac_real_dsym set, strip_from_xcode won't be used.
                # Specify CHROMIUM_STRIP_SAVE_FILE directly to Xcode.
                'STRIPFLAGS': '-s $(CHROMIUM_STRIP_SAVE_FILE)',
              },
              'dependencies': [
                '../breakpad/breakpad.gyp:dump_syms',
                '../breakpad/breakpad.gyp:symupload',
              ],
              # The "Dump Symbols" post-build step is in a target_conditions
              # block so that it will follow the "Strip If Needed" step if that
              # is also being used.  There is no standard configuration where
              # both of these steps occur together, but Mark likes to use this
              # configuration sometimes when testing Breakpad-enabled builds
              # without the time overhead of creating real .dSYM files.  When
              # both "Dump Symbols" and "Strip If Needed" are present, "Dump
              # Symbols" must come second because "Strip If Needed" creates
              # a fake .dSYM that dump_syms needs to fake dump.  Since
              # "Strip If Needed" is added in a target_conditions block in
              # common.gypi, "Dump Symbols" needs to be in an (always true)
              # target_conditions block.
              'target_conditions': [
                ['1 == 1', {
                  'postbuilds': [
                    {
                      'postbuild_name': 'Dump Symbols',
                      'variables': {
                        'dump_product_syms_path':
                            'tools/build/mac/dump_product_syms',
                      },
                      'action': ['<(dump_product_syms_path)',
                                 '<(branding)'],
                    },
                  ],
                }],
              ],
            }],  # mac_breakpad
          ],
          'product_name': '<(mac_product_name)',
          'xcode_settings': {
            # chrome/app/app-Info.plist has:
            #   CFBundleIdentifier of CHROMIUM_BUNDLE_ID
            #   CFBundleName of CHROMIUM_SHORT_NAME
            #   CFBundleSignature of CHROMIUM_CREATOR
            # Xcode then replaces these values with the branded values we set
            # as settings on the target.
            'CHROMIUM_BUNDLE_ID': '<(mac_bundle_id)',
            'CHROMIUM_CREATOR': '<(mac_creator)',
            'CHROMIUM_SHORT_NAME': '<(branding)',
          },
          'dependencies': [
            '../components/components.gyp:chrome_manifest_bundle',
            'helper_app',
            'infoplist_strings_tool',
            # On Mac, make sure we've built chrome_dll, which contains all of
            # the library code with Chromium functionality.
            'chrome_dll_dependency_shim',
          ],
          'mac_bundle_resources': [
            'app/theme/<(branding_path_component)/mac/app.icns',
            'app/theme/<(branding_path_component)/mac/document.icns',
            'browser/ui/cocoa/applescript/scripting.sdef',
            '<(PRODUCT_DIR)/<(mac_bundle_id).manifest',
          ],
          'actions': [
            {
              # Generate the InfoPlist.strings file
              'action_name': 'Generate InfoPlist.strings files',
              'variables': {
                'tool_path': '<(PRODUCT_DIR)/infoplist_strings_tool',
                # Unique dir to write to so the [lang].lproj/InfoPlist.strings
                # for the main app and the helper app don't name collide.
                'output_path': '<(INTERMEDIATE_DIR)/app_infoplist_strings',
                'branding_name': '<(branding_path_component)_strings',
              },
              'inputs': [
                '<(tool_path)',
                '<(version_path)',
                # TODO: remove this helper when we have loops in GYP
                '>!@(<(apply_locales_cmd) \'<(grit_out_dir)/<(branding_name)_ZZLOCALE.pak\' <(locales))',
              ],
              'outputs': [
                # TODO: remove this helper when we have loops in GYP
                '>!@(<(apply_locales_cmd) -d \'<(output_path)/ZZLOCALE.lproj/InfoPlist.strings\' <(locales))',
              ],
              'action': [
                '<(tool_path)',
                '-b', '<(branding_name)',
                '-v', '<(version_path)',
                '-g', '<(grit_out_dir)',
                '-o', '<(output_path)',
                '-t', 'main',
                '<@(locales)',
              ],
              'message': 'Generating the language InfoPlist.strings files',
              'process_outputs_as_mac_bundle_resources': 1,
            },
          ],
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/<(mac_product_name).app/Contents/Versions/<(version_full)',
              'files': [
                '<(PRODUCT_DIR)/<(mac_product_name) Helper.app',
              ],
            },
          ],
          'postbuilds': [
            {
              'postbuild_name': 'Copy <(mac_product_name) Framework.framework',
              'action': [
                '../build/mac/copy_framework_unversioned.sh',
                '${BUILT_PRODUCTS_DIR}/<(mac_product_name) Framework.framework',
                '${BUILT_PRODUCTS_DIR}/${CONTENTS_FOLDER_PATH}/Versions/<(version_full)',
              ],
            },
            {
              # Modify the Info.plist as needed.  The script explains why this
              # is needed.  This is also done in the helper_app and chrome_dll
              # targets.  Use --breakpad=0 to not include any Breakpad
              # information; that all goes into the framework's Info.plist.
              # Keystone information is included if Keystone is enabled.  The
              # application reads Keystone keys from this plist and not the
              # framework's, and the ticket will reference this Info.plist to
              # determine the tag of the installed product.  Use --scm=1 to
              # include SCM information.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['<(tweak_info_plist_path)',
                         '--plist=${TARGET_BUILD_DIR}/${INFOPLIST_PATH}',
                         '--breakpad=0',
                         '--keystone=<(mac_keystone)',
                         '--scm=1',
                         '--bundle_id=<(mac_bundle_id)'],
            },
            {
              'postbuild_name': 'Clean up old versions',
              'action': [
                'tools/build/mac/clean_up_old_versions',
                '<(version_full)'
              ],
            },
          ],  # postbuilds
        }, {  # OS != "mac"
          'conditions': [
            # TODO:  add a:
            #   'product_name': 'chromium'
            # whenever we convert the rest of the infrastructure
            # (buildbots etc.) to understand the branding gyp define.
            # NOTE: chrome/app/theme/chromium/BRANDING and
            # chrome/app/theme/google_chrome/BRANDING have the short name
            # "chrome" etc.; should we try to extract from there instead?
          ],
          'dependencies': [
            '../components/components.gyp:startup_metric_utils_browser',
            'chrome_resources.gyp:packed_extra_resources',
            'chrome_resources.gyp:packed_resources',
            # Copy Flash Player files to PRODUCT_DIR if applicable. Let the .gyp
            # file decide what to do on a per-OS basis; on Mac, internal plugins
            # go inside the framework, so this dependency is in chrome_dll.gypi.
            '../third_party/adobe/flash/flash_player.gyp:flapper_binaries',
            # Copy CDM files to PRODUCT_DIR if applicable. Let the .gyp
            # file decide what to do on a per-OS basis; on Mac, internal plugins
            # go inside the framework, so this dependency is in chrome_dll.gypi.
            '../third_party/widevine/cdm/widevine_cdm.gyp:widevinecdmadapter',
          ],
        }],
        ['chrome_multiple_dll', {
          'defines': ['CHROME_MULTIPLE_DLL'],
        }],
        ['OS=="linux"', {
          'conditions': [
            ['branding=="Chrome"', {
              'dependencies': [
                'linux_installer_configs',
              ],
            }],
            # For now, do not build nacl_helper when disable_nacl=1
            # http://code.google.com/p/gyp/issues/detail?id=239
            ['disable_nacl==0', {
              'dependencies': [
                '../native_client/src/trusted/service_runtime/linux/nacl_bootstrap.gyp:nacl_helper_bootstrap',
                '../components/nacl.gyp:nacl_helper',
                '../components/nacl_nonsfi.gyp:nacl_helper_nonsfi',
              ],
            }],
          ],
          'dependencies': [
            '../sandbox/sandbox.gyp:sandbox',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            'chrome_dll',
            'chrome_nacl_win64',
            'chrome_process_finder',
            'chrome_version_resources',
            'chrome_watcher',
            'chrome_watcher_client',
            'file_pre_reader',
            'installer_util',
            'install_static_util',
            'metrics_constants_util_win',
            'visual_elements_resources',
            '../base/base.gyp:base',
            '../breakpad/breakpad.gyp:breakpad_handler',
            '../breakpad/breakpad.gyp:breakpad_sender',
            '../chrome/common_constants.gyp:common_constants',
            '../chrome_elf/chrome_elf.gyp:chrome_elf',
            '../components/components.gyp:browser_watcher_client',
            '../components/components.gyp:crash_component',
            '../components/components.gyp:crash_core_common',
            '../components/components.gyp:flags_ui_switches',
            '../components/components.gyp:policy',
            '../components/components.gyp:startup_metric_utils_win',
            '../crypto/crypto.gyp:crypto',
            '../sandbox/sandbox.gyp:sandbox',
            '../ui/gfx/gfx.gyp:gfx',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/chrome_exe_version.rc',
            'app/chrome_crash_reporter_client_win.cc',
            'app/chrome_crash_reporter_client_win.h',
            'app/chrome_exe.rc',
            'common/crash_keys.cc',
            'common/crash_keys.h',
          ],
          'sources!': [
            # We still want the _win entry point for sandbox, etc.
            'app/chrome_exe_main_aura.cc',
          ],
          'conditions': [
            ['win_console_app==1', {
              'defines': ['WIN_CONSOLE_APP'],
            }],
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'OutputFile': '$(OutDir)\\initialexe\\chrome.exe',
              'DelayLoadDLLs': [
                'dbghelp.dll',
                'dwmapi.dll',
                'uxtheme.dll',
                'ole32.dll',
                'oleaut32.dll',
              ],
              'conditions': [
                ['win_console_app==0', {
                  # Set /SUBSYSTEM:WINDOWS for chrome.exe itself, unless a
                  # console build has been requested.
                  'SubSystem': '2',
                }],
              ],
            },
            'VCManifestTool': {
              'AdditionalManifestFiles': [
                '$(ProjectDir)\\app\\chrome.exe.manifest',
                '<(SHARED_INTERMEDIATE_DIR)/chrome/app/version_assembly/version_assembly.manifest',
              ],
            },
          },
          'actions': [
            {
              'action_name': 'first_run',
              'inputs': [
                  'app/FirstRun',
              ],
              'outputs': [
                  '<(PRODUCT_DIR)/First Run',
              ],
              'action': ['python', '../build/cp.py', '<@(_inputs)', '<@(_outputs)'],
              'message': 'Copy first run complete sentinel file',
            },
            {
              # GN version: //chrome/app/version_assembly:chrome_exe_manifest
              'action_name': 'chrome_exe_manifest',
              'includes': [
                  'app/version_assembly/chrome_exe_manifest_action.gypi',
              ],
            },
            {
              # GN version: //chrome/app/version_assembly:version_assembly_manifest
              'action_name': 'version_assembly_manifest',
              'includes': [
                  'app/version_assembly/version_assembly_manifest_action.gypi',
              ],
            },
          ],
        }],
        ['OS=="win" and component=="shared_library"', {
          'defines': ['COMPILE_CONTENT_STATICALLY'],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'file_pre_reader',
          'type': 'static_library',
          'sources': [
            'app/file_pre_reader_win.cc',
            'app/file_pre_reader_win.h',
          ],
          'dependencies': [
             '../base/base.gyp:base',
             '../components/components.gyp:startup_metric_utils_win',
          ],
        },
        {
          'target_name': 'visual_elements_resources',
          'type': 'none',
          'copies': [
            {
              # GN version: //chrome/visual_elements_resources
              'destination': '<(PRODUCT_DIR)',
              'files': [
                'app/visual_elements_resources/Logo.png',
                'app/visual_elements_resources/SecondaryTile.png',
                'app/visual_elements_resources/SmallLogo.png',
                'app/visual_elements_resources/chrome.VisualElementsManifest.xml',
              ],
            },
          ],
        },
      ],
      'conditions': [
        ['disable_nacl!=1 and target_arch=="ia32"', {
          'targets': [
            {
              'target_name': 'chrome_nacl_win64',
              'type': 'executable',
              'product_name': 'nacl64',
              'sources': [
                '../content/app/sandbox_helper_win.cc',
                '../content/common/sandbox_init_win.cc',
                '../content/common/sandbox_win.cc',
                '../content/public/common/content_switches.cc',
                '../content/public/common/sandboxed_process_launcher_delegate.cc',
                '<(SHARED_INTERMEDIATE_DIR)/chrome_version/nacl64_exe_version.rc',
                'app/chrome_crash_reporter_client_win.cc',
                'common/crash_keys.cc',
                'nacl/nacl_exe_win_64.cc',
              ],
              'dependencies': [
                'chrome_version_resources',
                'install_static_util_nacl_win64',
                'installer_util_nacl_win64',
                '../base/base.gyp:base_i18n_nacl_win64',
                '../base/base.gyp:base_win64',
                '../base/base.gyp:base_static_win64',
                '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations_win64',
                '../breakpad/breakpad.gyp:breakpad_handler_win64',
                '../breakpad/breakpad.gyp:breakpad_sender_win64',
                '../components/components.gyp:breakpad_win64',
                '../components/components.gyp:crash_core_common_win64',
                '../components/components.gyp:flags_ui_switches_win64',
                '../components/components.gyp:policy_win64',
                '../chrome/common_constants.gyp:common_constants_win64',
                '../components/nacl.gyp:nacl_win64',
                '../crypto/crypto.gyp:crypto_nacl_win64',
                '../ipc/ipc.gyp:ipc_win64',
                '../sandbox/sandbox.gyp:sandbox_win64',
                '../third_party/kasko/kasko.gyp:kasko',
              ],
              'defines': [
                '<@(nacl_win64_defines)',
                'COMPILE_CONTENT_STATICALLY',
              ],
              'include_dirs': [
                '<(SHARED_INTERMEDIATE_DIR)/chrome',
              ],
              'msvs_settings': {
                'VCLinkerTool': {
                  'ImportLibrary': '$(OutDir)\\lib\\nacl64_exe.lib',
                  'SubSystem': '2',         # Set /SUBSYSTEM:WINDOWS
                },
              },
              'configurations': {
                'Common_Base': {
                  'msvs_target_platform': 'x64',
                },
              },
            },
          ],
        }, {  # else (disable_nacl==1)
          'targets': [
            {
              'target_name': 'chrome_nacl_win64',
              'type': 'none',
              'sources': [],
            },
          ],
        }],
      ],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'chrome_run',
          'type': 'none',
          'dependencies': [
            'chrome',
            # Runtime dependencies
            '../third_party/mesa/mesa.gyp:osmesa',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'chrome.isolate',
          ],
          'conditions': [
            ['OS=="win"', {
              'dependencies': [
                'chrome_nacl_win64',
              ],
            }],
          ],
        },
      ],
    }],
  ],
}
