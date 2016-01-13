# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
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
      'target_name': 'chrome_initial',
      'type': 'executable',
      # Name the exe chrome.exe, not chrome_initial.exe.
      'product_name': 'chrome',
      'mac_bundle': 1,
      'variables': {
        'use_system_xdg_utils%': 0,
        'enable_wexit_time_destructors': 1,
      },
      'sources': [
        'app/chrome_exe_main_aura.cc',
        'app/chrome_exe_main_mac.cc',
        'app/chrome_exe_main_win.cc',
        'app/chrome_exe_resource.h',
        'app/client_util.cc',
        'app/client_util.h',
        'app/signature_validator_win.cc',
        'app/signature_validator_win.h',
        '<(DEPTH)/content/app/startup_helper_win.cc',
        '<(DEPTH)/content/public/common/content_switches.cc',
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
        ['order_profiling!=0 and (chromeos==1 or OS=="linux")', {
          'dependencies' : [
            '../tools/cygprofile/cygprofile.gyp:cygprofile',
          ],
        }],
        ['order_text_section!=""', {
          'target_conditions' : [
            ['_toolset=="target"', {
              'ldflags': [
                '-Wl,-section-ordering-file=<(order_text_section)' ],
            }],
          ]
        }],
        ['OS == "win"', {
          'sources!': [
            # We still want the _win entry point for sandbox, etc.
            'app/chrome_exe_main_aura.cc',
          ],
          'dependencies': [
            '../ui/gfx/gfx.gyp:gfx',
          ],
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
                'tools/build/linux/sed.sh',
                'app/resources/manpage.1.in',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/chrome.1',
              ],
              'action': [
                'tools/build/linux/sed.sh',
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
            ['use_allocator!="none"', {
                'dependencies': [
                  '<(allocator_target)',
                ],
              },
            ],
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
                            ],
                  # The wrapper script above may need to generate a .desktop
                  # file, which requires an icon. So, copy one next to the
                  # script.
                  'conditions': [
                    ['branding=="Chrome"', {
                      'files': ['app/theme/google_chrome/product_logo_48.png']
                    }, { # else: 'branding!="Chrome"
                      'files': ['app/theme/chromium/product_logo_48.png']
                    }],
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
            # Needed to use the master_preferences functions
            'installer_util',
          ],
        }],
        ['OS=="mac"', {
          # 'branding' is a variable defined in common.gypi
          # (e.g. "Chromium", "Chrome")
          'conditions': [
            ['branding=="Chrome"', {
              'mac_bundle_resources': [
                'app/theme/google_chrome/mac/app.icns',
                'app/theme/google_chrome/mac/document.icns',
                'browser/ui/cocoa/applescript/scripting.sdef',
              ],
            }, {  # else: 'branding!="Chrome"
              'mac_bundle_resources': [
                'app/theme/chromium/mac/app.icns',
                'app/theme/chromium/mac/document.icns',
                'browser/ui/cocoa/applescript/scripting.sdef',
              ],
            }],
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
            'interpose_dependency_shim',
            # On Mac, make sure we've built chrome_dll, which contains all of
            # the library code with Chromium functionality.
            'chrome_dll',
          ],
          'mac_bundle_resources': [
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
              },
              'conditions': [
                [ 'branding == "Chrome"', {
                  'variables': {
                     'branding_name': 'google_chrome_strings',
                  },
                }, { # else branding!="Chrome"
                  'variables': {
                     'branding_name': 'chromium_strings',
                  },
                }],
              ],
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
                '<(PRODUCT_DIR)/libplugin_carbon_interpose.dylib',
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
            {
              # This postbuid step is responsible for creating the following
              # helpers:
              #
              # For unofficial Chromium branding, Chromium Helper EH.app and
              # Chromium Helper NP.app are created from Chromium Helper.app.
              # For official Google Chrome branding, Google Chrome Helper
              # EH.app and Google Chrome Helper NP.app are created from
              # Google Chrome Helper.app.
              #
              # The EH helper is marked for an executable heap. The NP helper
              # is marked for no PIE (ASLR).
              #
              # Normally, applications shipping as part of offical builds with
              # Google Chrome branding have dsymutil (dwarf-with-dsym,
              # mac_real_dsym) and dump_syms (mac_breakpad) run on them to
              # produce a .dSYM bundle and a Breakpad .sym file. This is
              # unnecessary for the "More Helpers" because they're identical
              # to the original helper except for the bits in their Mach-O
              # headers that change to enable or disable special features.
              # Each .dSYM is identified by UUID stored in a Mach-O file's
              # LC_UUID load command. Because the "More Helpers" share a UUID
              # with the original helper, there's no need to run dsymutil
              # again. All helpers can share the same .dSYM. Special handling
              # is performed in chrome/tools/build/mac/dump_product_syms to
              # prepare their Breakpad symbol files.
              'postbuild_name': 'Make More Helpers',
              'action': [
                '../build/mac/make_more_helpers.sh',
                'Versions/<(version_full)',
                '<(mac_product_name)',
              ],
            },
            {
              # Make sure there isn't any Objective-C in the browser app's
              # executable.
              'postbuild_name': 'Verify No Objective-C',
              'action': [
                '../build/mac/verify_no_objc.sh',
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

            # CrOS does this in a separate build step.
            ['OS=="linux" and chromeos==0 and linux_dump_symbols==1', {
              'dependencies': [
                '../pdf/pdf.gyp:pdf_linux_symbols',
              ],
            }], # OS=="linux" and chromeos==0 and linux_dump_symbols==1
            # Android doesn't use pdfium.
            ['OS!="android"', {
              'dependencies': [
                # On Mac, this is done in chrome_dll.gypi.
                '../pdf/pdf.gyp:pdf',
              ],
            }], # OS=="android"
          ],
          'dependencies': [
            '../components/components.gyp:startup_metric_utils',
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
        ['OS=="mac" and asan==1', {
          'xcode_settings': {
            # Override the outer definition of CHROMIUM_STRIP_SAVE_FILE.
            'CHROMIUM_STRIP_SAVE_FILE': 'app/app_asan.saves',
          },
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
            'installer_util',
            'image_pre_reader',
            '../base/base.gyp:base',
            '../breakpad/breakpad.gyp:breakpad_handler',
            '../breakpad/breakpad.gyp:breakpad_sender',
            '../chrome_elf/chrome_elf.gyp:chrome_elf',
            '../components/components.gyp:breakpad_component',
            '../sandbox/sandbox.gyp:sandbox',
          ],
          'sources': [
            'app/chrome_breakpad_client.cc',
            'app/chrome_breakpad_client.h',
            'app/chrome_exe.rc',
            'common/crash_keys.cc',
            'common/crash_keys.h',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/chrome_exe_version.rc',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'ImportLibrary': '$(OutDir)\\lib\\chrome_exe.lib',
              'OutputFile': '$(OutDir)\\initialexe\\chrome.exe',
              'DelayLoadDLLs': [
                'dbghelp.dll',
                'dwmapi.dll',
                'uxtheme.dll',
                'ole32.dll',
                'oleaut32.dll',
              ],
              'AdditionalDependencies': [ 'wintrust.lib' ],
              'conditions': [
                ['asan==0', {
                  # Set /SUBSYSTEM:WINDOWS for chrome.exe itself, except for the
                  # AddressSanitizer build where console output is important.
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
          'conditions': [
            ['configuration_policy==1', {
              'dependencies': [
                '<(DEPTH)/components/components.gyp:policy',
              ],
            }],
          ],
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
              'action_name': 'chrome_exe_manifest',
              'includes': [
                  'app/version_assembly/chrome_exe_manifest_action.gypi',
              ],
            },
            {
              'action_name': 'version_assembly_manifest',
              'includes': [
                  'app/version_assembly/version_assembly_manifest_action.gypi',
              ],
            },
          ],
        }, {  # 'OS!="win"
          'sources!': [
            'app/client_util.cc',
          ],
        }],
        ['OS=="win" and component=="shared_library"', {
          'defines': ['COMPILE_CONTENT_STATICALLY'],
        }],
        ['OS=="win"', {
          'dependencies': [
            '../win8/metro_driver/metro_driver.gyp:metro_driver',
            '../win8/delegate_execute/delegate_execute.gyp:*',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'image_pre_reader',
          'type': 'static_library',
          'sources': [
            'app/image_pre_reader_win.cc',
            'app/image_pre_reader_win.h',
          ],
          'dependencies': [
             '../base/base.gyp:base',
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
                'app/chrome_breakpad_client.cc',
                'common/crash_keys.cc',
                'nacl/nacl_exe_win_64.cc',
                '../content/app/startup_helper_win.cc',
                '../content/common/sandbox_init_win.cc',
                '../content/common/sandbox_win.cc',
                '../content/public/common/content_switches.cc',
                '../content/public/common/sandboxed_process_launcher_delegate.cc',
                '<(SHARED_INTERMEDIATE_DIR)/chrome_version/nacl64_exe_version.rc',
              ],
              'dependencies': [
                'chrome_version_resources',
                'installer_util_nacl_win64',
                '../base/base.gyp:base_i18n_nacl_win64',
                '../base/base.gyp:base_win64',
                '../base/base.gyp:base_static_win64',
                '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations_win64',
                '../breakpad/breakpad.gyp:breakpad_handler_win64',
                '../breakpad/breakpad.gyp:breakpad_sender_win64',
                '../components/components.gyp:breakpad_win64',
                '../chrome/common_constants.gyp:common_constants_win64',
                '../components/nacl.gyp:nacl_win64',
                '../crypto/crypto.gyp:crypto_nacl_win64',
                '../ipc/ipc.gyp:ipc_win64',
                '../sandbox/sandbox.gyp:sandbox_win64',
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
              'conditions': [
                ['configuration_policy==1', {
                  'dependencies': [
                    '<(DEPTH)/components/components.gyp:policy_win64',
                  ],
                }],
              ],
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
          ],
          'includes': [
            '../build/isolate.gypi',
            'chrome.isolate',
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
