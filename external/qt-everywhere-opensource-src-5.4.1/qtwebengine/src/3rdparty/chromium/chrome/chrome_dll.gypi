# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'conditions': [
    ['OS=="mac" or OS=="win"', {
      'targets': [
        {
          'target_name': 'chrome_dll',
          'type': 'none',
          'dependencies': [
            'chrome_main_dll',
          ],
          'conditions': [
            ['OS=="mac" and component=="shared_library"', {
              'type': 'shared_library',
              'includes': [ 'chrome_dll_bundle.gypi' ],
              'xcode_settings': {
                'OTHER_LDFLAGS': [
                  '-Wl,-reexport_library,<(PRODUCT_DIR)/libchrome_main_dll.dylib',
                ],
              },
            }],  # OS=="mac"
            ['chrome_multiple_dll==1', {
              'dependencies': [
                'chrome_child_dll',
              ],
            }],
            ['incremental_chrome_dll==1', {
              # Linking to a different directory and then hardlinking back
              # to OutDir is a workaround to avoid having the .ilk for
              # chrome.exe and chrome.dll conflicting. See crbug.com/92528
              # for more information. Done on the dll instead of the exe so
              # that people launching from VS don't need to modify
              # $(TargetPath) for the exe.
              'actions': [
                {
                  'action_name': 'hardlink_to_output',
                  'inputs': [
                    '$(OutDir)\\initial\\chrome.dll',
                  ],
                  'outputs': [
                    '$(OutDir)\\chrome.dll',
                  ],
                  'action': ['tools\\build\\win\\hardlink_failsafe.bat',
                             '$(OutDir)\\initial\\chrome.dll',
                             '$(OutDir)\\chrome.dll'],
                },
              ],
              'conditions': [
                # Only hardlink pdb if we're generating debug info.
                ['fastbuild==0 or win_z7!=0', {
                  'actions': [
                    {
                      'action_name': 'hardlink_pdb_to_output',
                      'inputs': [
                        # Not the pdb, since gyp doesn't know about it
                        '$(OutDir)\\initial\\chrome.dll',
                      ],
                      'outputs': [
                        '$(OutDir)\\chrome.dll.pdb',
                      ],
                      'action': ['tools\\build\\win\\hardlink_failsafe.bat',
                                 '$(OutDir)\\initial\\chrome.dll.pdb',
                                 '$(OutDir)\\chrome.dll.pdb'],
                    }
                  ]
                }]
              ],
            }],
          ]
        },
        {
          'target_name': 'chrome_main_dll',
          'type': 'shared_library',
          'variables': {
            'enable_wexit_time_destructors': 1,
          },
          'dependencies': [
            '<@(chromium_browser_dependencies)',
            '../content/content.gyp:content_app_browser',
          ],
          'conditions': [
            ['OS=="win"', {
              'dependencies': [
                '<(DEPTH)/chrome_elf/chrome_elf.gyp:chrome_elf',
              ],
            }],
            ['OS=="win" and configuration_policy==1', {
              'dependencies': [
                '<(DEPTH)/components/components.gyp:policy',
              ],
            }],
            ['use_aura==1', {
              'dependencies': [
                '../ui/compositor/compositor.gyp:compositor',
              ],
            }],
            ['OS=="win" and target_arch=="ia32"', {
              # Add a dependency to custom import library for user32 delay
              # imports only in x86 builds.
              'dependencies': [
                'chrome_user32_delay_imports',
              ],
            },],
            ['OS=="win"', {
              'product_name': 'chrome',
              'dependencies': [
                # On Windows, link the dependencies (libraries) that make
                # up actual Chromium functionality into this .dll.
                'chrome_version_resources',
                '../chrome/chrome_resources.gyp:chrome_unscaled_resources',
                '../crypto/crypto.gyp:crypto',
                '../net/net.gyp:net_resources',
                '../ui/views/views.gyp:views',
                '../webkit/webkit_resources.gyp:webkit_resources',
              ],
              'sources': [
                'app/chrome_command_ids.h',
                'app/chrome_dll.rc',
                'app/chrome_dll_resource.h',
                'app/chrome_main.cc',
                'app/chrome_main_delegate.cc',
                'app/chrome_main_delegate.h',
                'app/delay_load_hook_win.cc',
                'app/delay_load_hook_win.h',

                '<(SHARED_INTERMEDIATE_DIR)/chrome_version/chrome_dll_version.rc',
                '../base/win/dllmain.cc',

                # Cursors.
                '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_unscaled_resources.rc',
              ],
              'include_dirs': [
                '<(DEPTH)/third_party/wtl/include',
              ],
              'configurations': {
                'Debug_Base': {
                  'msvs_settings': {
                    'VCLinkerTool': {
                      'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                    },
                  },
                },
              },
              'msvs_settings': {
                'VCLinkerTool': {
                  'ImportLibrary': '$(OutDir)\\lib\\chrome_dll.lib',
                  # Set /SUBSYSTEM:WINDOWS for chrome.dll (for consistency).
                  'SubSystem': '2',
                  'conditions': [
                    ['incremental_chrome_dll==1', {
                      'OutputFile': '$(OutDir)\\initial\\chrome.dll',
                      'UseLibraryDependencyInputs': "true",
                    }],
                    ['target_arch=="ia32"', {
                      # Don't set an x64 base address (to avoid breaking HE-ASLR).
                      'BaseAddress': '0x01c30000',
                      # Link against the XP-constrained user32 import library
                      # instead of the platform-SDK provided one to avoid
                      # inadvertently taking dependencies on post-XP user32
                      # exports.
                      'AdditionalDependencies!': [
                        'user32.lib',
                      ],
                      'IgnoreDefaultLibraryNames': [
                        'user32.lib',
                      ],
                      # Remove user32 delay load for chrome.dll.
                      'DelayLoadDLLs!': [
                        'user32.dll',
                      ],
                      'AdditionalDependencies': [
                        'user32.winxp.lib',
                      ],
                      'DelayLoadDLLs': [
                        'user32-delay.dll',
                      ],
                      'AdditionalLibraryDirectories': [
                        '<(DEPTH)/build/win/importlibs/x86',
                      ],
                      'ForceSymbolReferences': [
                        # Force the inclusion of the delay load hook in this
                        # binary.
                        '_ChromeDelayLoadHook@8',
                      ],
                    }],
                  ],
                  'DelayLoadDLLs': [
                    'comdlg32.dll',
                    'crypt32.dll',
                    'cryptui.dll',
                    'dhcpcsvc.dll',
                    'imagehlp.dll',
                    'imm32.dll',
                    'iphlpapi.dll',
                    'setupapi.dll',
                    'urlmon.dll',
                    'winhttp.dll',
                    'wininet.dll',
                    'winspool.drv',
                    'ws2_32.dll',
                    'wsock32.dll',
                  ],
                },
                'VCManifestTool': {
                  'AdditionalManifestFiles': [
                    '$(ProjectDir)\\app\\chrome.dll.manifest',
                  ],
                },
              },
              'conditions': [
                ['win_use_allocator_shim==1', {
                  'dependencies': [
                    '<(allocator_target)',
                  ],
                }],
                ['enable_printing!=0', {
                  'dependencies': [
                    '../printing/printing.gyp:printing',
                  ],
                }],
                ['chrome_pgo_phase==1', {
                  'msvs_settings': {
                    'VCLinkerTool': {
                      'LinkTimeCodeGeneration': '2',
                    },
                  },
                }],
                ['chrome_pgo_phase==2', {
                  'msvs_settings': {
                    'VCLinkerTool': {
                      'LinkTimeCodeGeneration': '3',
                    },
                  },
                }],
              ]
            }],
            ['chrome_multiple_dll==1', {
              'defines': [
                'CHROME_MULTIPLE_DLL_BROWSER',
              ],
            }, {
              'dependencies': [
                '<@(chromium_child_dependencies)',
                '../content/content.gyp:content_app_both',
                '../content/content.gyp:content_worker',
              ],
              'dependencies!': [
                '../content/content.gyp:content_app_browser',
              ],
            }],
            ['cld_version==0 or cld_version==1', {
              'dependencies': [
                '../third_party/cld/cld.gyp:cld',
              ],
            }],
            ['cld_version==0 or cld_version==2', {
              'dependencies': [
                '../third_party/cld_2/cld_2.gyp:cld_2',
              ],
            }],
            ['OS=="mac" and component!="shared_library"', {
              'includes': [ 'chrome_dll_bundle.gypi' ],
            }],
            ['OS=="mac" and component=="shared_library"', {
              'xcode_settings': { 'OTHER_LDFLAGS': [ '-Wl,-ObjC' ], },
            }],
            ['OS=="mac"', {
              'xcode_settings': {
                # Define the order of symbols within the framework.  This
                # sets -order_file.
                'ORDER_FILE': 'app/framework.order',
              },
              'sources': [
                'app/chrome_command_ids.h',
                'app/chrome_dll_resource.h',
                'app/chrome_main.cc',
                'app/chrome_main_delegate.cc',
                'app/chrome_main_delegate.h',
                'app/chrome_main_mac.mm',
                'app/chrome_main_mac.h',
              ],
              'dependencies': [
                '../pdf/pdf.gyp:pdf',
              ],
              'include_dirs': [
                '<(grit_out_dir)',
              ],
              'postbuilds': [
                {
                  # This step causes an error to be raised if the .order file
                  # does not account for all global text symbols.  It
                  # validates the completeness of the .order file.
                  'postbuild_name': 'Verify global text symbol order',
                  'variables': {
                    'verify_order_path': 'tools/build/mac/verify_order',
                  },
                  'action': [
                    '<(verify_order_path)',
                    '_ChromeMain',
                    '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}',
                  ],
                },
              ],
              'conditions': [
                ['mac_breakpad_compiled_in==1', {
                  'dependencies': [
                    '../breakpad/breakpad.gyp:breakpad',
                    '../components/components.gyp:breakpad_component',
                    '../components/components.gyp:policy',
                  ],
                  'sources': [
                    'app/chrome_breakpad_client.cc',
                    'app/chrome_breakpad_client.h',
                    'app/chrome_breakpad_client_mac.mm',
                  ],
                }, {  # else: mac_breakpad_compiled_in!=1
                  # No Breakpad, put in the stubs.
                  'dependencies': [
                    '../components/components.gyp:breakpad_stubs',
                  ],
                }],  # mac_breakpad_compiled_in
              ],  # conditions
            }],  # OS=="mac"
          ],  # conditions
        },  # target chrome_main_dll
      ],  # targets
    }],  # OS=="mac" or OS=="win"
    ['chrome_multiple_dll', {
      'targets': [
        {
          'target_name': 'chrome_child_dll',
          'type': 'shared_library',
          'product_name': 'chrome_child',
          'variables': {
            'enable_wexit_time_destructors': 1,
          },
          'dependencies': [
            '<@(chromium_child_dependencies)',
            '../content/content.gyp:content_app_child',
            '../content/content.gyp:content_worker',
            'chrome_version_resources',
            'policy_path_parser',
          ],
          'defines': [
            'CHROME_MULTIPLE_DLL_CHILD',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/chrome_dll_version.rc',
            'app/chrome_main.cc',
            'app/chrome_main_delegate.cc',
            'app/chrome_main_delegate.h',
          ],
          'conditions': [
            ['OS=="win"', {
              'dependencies': [
                '<(DEPTH)/chrome_elf/chrome_elf.gyp:chrome_elf',
              ],
              'conditions': [
                ['chrome_pgo_phase==1', {
                  'msvs_settings': {
                    'VCLinkerTool': {
                      'LinkTimeCodeGeneration': '2',
                    },
                  },
                }],
                ['chrome_pgo_phase==2', {
                  'msvs_settings': {
                    'VCLinkerTool': {
                      'LinkTimeCodeGeneration': '3',
                    },
                  },
                }],
              ]
            }],
          ],
        },  # target chrome_child_dll
      ],
    }],
  ],
}
