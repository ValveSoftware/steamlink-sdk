{
  'variables': {
    'version_py': '<(DEPTH)/build/util/version.py',
    'version_path': '../../chrome/VERSION',
    'lastchange_path': '<(DEPTH)/build/util/LASTCHANGE',
    # 'branding_dir' is set in the 'conditions' section at the bottom.
    'msvs_use_common_release': 0,
    'msvs_use_common_linker_extras': 0,
    'mini_installer_internal_deps%': 0,
    'mini_installer_official_deps%': 0,
  },
  'includes': [
    '../../build/win_precompile.gypi',
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'mini_installer',
          'type': 'executable',

          'dependencies': [
            '../chrome.gyp:chrome',
            '../chrome.gyp:chrome_nacl_win64',
            '../chrome.gyp:chrome_dll',
            '../chrome.gyp:default_extensions',
            '../chrome.gyp:setup',
          ],
          'include_dirs': [
            '../..',
            '<(INTERMEDIATE_DIR)',
            '<(SHARED_INTERMEDIATE_DIR)/chrome',
          ],
          'sources': [
            'mini_installer/appid.h',
            'mini_installer/chrome.release',
            'mini_installer/chrome_appid.cc',
            'mini_installer/configuration.cc',
            'mini_installer/configuration.h',
            'mini_installer/decompress.cc',
            'mini_installer/decompress.h',
            'mini_installer/mini_installer.cc',
            'mini_installer/mini_installer.h',
            'mini_installer/mini_installer.ico',
            'mini_installer/mini_installer.rc',
            'mini_installer/mini_installer_exe_version.rc.version',
            'mini_installer/mini_installer_resource.h',
            'mini_installer/mini_string.cc',
            'mini_installer/mini_string.h',
            'mini_installer/pe_resource.cc',
            'mini_installer/pe_resource.h',
            '<(INTERMEDIATE_DIR)/packed_files.rc',
          ],
          'msvs_settings': {
            'VCCLCompilerTool': {
              'EnableIntrinsicFunctions': 'true',
              'BufferSecurityCheck': 'false',
              'BasicRuntimeChecks': '0',
              'ExceptionHandling': '0',
            },
            'VCLinkerTool': {
              'RandomizedBaseAddress': '1',
              'DataExecutionPrevention': '0',
              'AdditionalLibraryDirectories': [
                '<(PRODUCT_DIR)/lib'
              ],
              'DelayLoadDLLs': [],
              'EntryPointSymbol': 'MainEntryPoint',
              'IgnoreAllDefaultLibraries': 'true',
              'OptimizeForWindows98': '1',
              'SubSystem': '2',     # Set /SUBSYSTEM:WINDOWS
              'AdditionalDependencies': [
                'shlwapi.lib',
                'setupapi.lib',
              ],
            },
            'VCManifestTool': {
              'AdditionalManifestFiles': [
                '$(ProjectDir)\\mini_installer\\mini_installer.exe.manifest',
              ],
            },
          },
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'BasicRuntimeChecks': '0',
                  'BufferSecurityCheck': 'false',
                  'ExceptionHandling': '0',
                },
                'VCLinkerTool': {
                  'SubSystem': '2',     # Set /SUBSYSTEM:WINDOWS
                  'AdditionalOptions': [
                    '/safeseh:no',
                    '/dynamicbase:no',
                    '/ignore:4199',
                    '/ignore:4221',
                    '/nxcompat',
                  ],
                },
              },
            },
            'Release_Base': {
              'includes': ['../../build/internal/release_defaults.gypi'],
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'EnableIntrinsicFunctions': 'true',
                  'BasicRuntimeChecks': '0',
                  'BufferSecurityCheck': 'false',
                  'ExceptionHandling': '0',
                },
                'VCLinkerTool': {
                  'SubSystem': '2',     # Set /SUBSYSTEM:WINDOWS
                  'Profile': 'false',   # Conflicts with /FIXED
                  'AdditionalOptions': [
                    '/SAFESEH:NO',
                    '/NXCOMPAT',
                    '/DYNAMICBASE:NO',
                    '/FIXED',
                  ],
                },
              },
            },
          },

          # Disable precompiled headers for this project, to avoid
          # linker errors when building with VS 2008.
          'msvs_precompiled_header': '',
          'msvs_precompiled_source': '',

          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],

          'variables': {
            # Opt out the common compatibility manifest to work around
            # crbug.com/272660.
            # TODO(yukawa): Enable the common compatibility manifest again.
            'win_exe_compatibility_manifest': '',
          },
          'rules': [
            {
              'rule_name': 'mini_installer_version',
              'extension': 'version',
              'variables': {
                'template_input_path': 'mini_installer/mini_installer_exe_version.rc.version',
              },
              'inputs': [
                '<(template_input_path)',
                '<(version_path)',
                '<(lastchange_path)',
                '<(branding_dir)/BRANDING',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/mini_installer_exe_version.rc',
              ],
              'action': [
                'python', '<(version_py)',
                '-f', '<(version_path)',
                '-f', '<(lastchange_path)',
                '-f', '<(branding_dir)/BRANDING',
                '<(template_input_path)',
                '<@(_outputs)',
              ],
              'process_outputs_as_sources': 1,
              'message': 'Generating version information'
            },
            {
              'rule_name': 'installer_archive',
              'extension': 'release',
              'variables': {
                'create_installer_archive_py_path':
                  '../tools/build/win/create_installer_archive.py',
              },
              'conditions': [
                ['enable_hidpi == 1', {
                  'variables': {
                    'enable_hidpi_flag': '--enable_hidpi=1',
                  },
                }, {
                  'variables': {
                    'enable_hidpi_flag': '',
                  },
                }],
                ['component == "shared_library"', {
                  'variables': {
                    'component_build_flag': '--component_build=1',
                  },
                }, {
                  'variables': {
                    'component_build_flag': '',
                  },
                  'outputs': [
                    '<(PRODUCT_DIR)/<(RULE_INPUT_NAME).packed.7z',
                  ],
                }],
                ['disable_nacl==1', {
                  'inputs!': [
                    '<(PRODUCT_DIR)/nacl64.exe',
                    '<(PRODUCT_DIR)/ppGoogleNaClPluginChrome.dll',
                    '<(PRODUCT_DIR)/nacl_irt_x86_32.nexe',
                    '<(PRODUCT_DIR)/nacl_irt_x86_64.nexe',
                  ],
                }],
                ['target_arch=="x64"', {
                  'inputs!': [
                    '<(PRODUCT_DIR)/nacl64.exe',
                    '<(PRODUCT_DIR)/nacl_irt_x86_32.nexe',
                  ],
                  'variables': {
                    'target_arch_flag': '--target_arch=x64',
                  },
                }, {
                  'variables': {
                    'target_arch_flag': '--target_arch=x86',
                  },
                }],
                ['icu_use_data_file_flag == 0', {
                  'inputs': [
                    '<(PRODUCT_DIR)/icudt.dll',
                  ],
                }, { # else icu_use_data_file_flag != 0
                  'inputs': [
                    '<(PRODUCT_DIR)/icudtl.dat',
                  ],
                }],
              ],
              'inputs': [
                '<(create_installer_archive_py_path)',
                '<(PRODUCT_DIR)/chrome.exe',
                '<(PRODUCT_DIR)/chrome.dll',
                '<(PRODUCT_DIR)/nacl64.exe',
                '<(PRODUCT_DIR)/ppGoogleNaClPluginChrome.dll',
                '<(PRODUCT_DIR)/nacl_irt_x86_32.nexe',
                '<(PRODUCT_DIR)/nacl_irt_x86_64.nexe',
                '<(PRODUCT_DIR)/locales/en-US.pak',
              ],
              'outputs': [
                # Also note that chrome.packed.7z is defined as an output in a
                # conditional above.
                'xxx2.out',
                '<(PRODUCT_DIR)/<(RULE_INPUT_NAME).7z',
                '<(PRODUCT_DIR)/setup.ex_',
                '<(INTERMEDIATE_DIR)/packed_files.rc',
              ],
              'action': [
                'python',
                '<(create_installer_archive_py_path)',
                '--build_dir', '<(PRODUCT_DIR)',
                '--staging_dir', '<(INTERMEDIATE_DIR)',
                '--input_file', '<(RULE_INPUT_PATH)',
                '--resource_file_path', '<(INTERMEDIATE_DIR)/packed_files.rc',
                '<(enable_hidpi_flag)',
                '<(component_build_flag)',
                '<(target_arch_flag)',
                # TODO(sgk):  may just use environment variables
                #'--distribution=$(CHROMIUM_BUILD)',
                '--distribution=_google_chrome',
                # Optional arguments to generate diff installer
                #'--last_chrome_installer=C:/Temp/base',
                #'--setup_exe_format=DIFF',
                #'--diff_algorithm=COURGETTE',
              ],
              'message': 'Create installer archive',
            },
          ],
          # TODO(mark):  <(branding_dir) should be defined by the
          # global condition block at the bottom of the file, but
          # this doesn't work due to the following issue:
          #
          #   http://code.google.com/p/gyp/issues/detail?id=22
          #
          # Remove this block once the above issue is fixed.
          'conditions': [
            [ 'branding == "Chrome"', {
              'variables': {
                 'branding_dir': '../app/theme/google_chrome',
              },
            }, { # else branding!="Chrome"
              'variables': {
                 'branding_dir': '../app/theme/chromium',
              },
            }],
          ],
        },
      ],
      'conditions': [
        ['test_isolation_mode != "noop"', {
          'targets': [
            {
              'target_name': 'mini_installer_tests_run',
              'type': 'none',
              'dependencies': [
                'mini_installer',
              ],
              'includes': [
                '../../build/isolate.gypi',
                'mini_installer_tests.isolate',
              ],
              'sources': [
                'mini_installer_tests.isolate',
              ],
            },
          ],
        }],
      ],
    }],
    [ 'branding == "Chrome"', {
      'variables': {
         'branding_dir': '../app/theme/google_chrome',
      },
    }, { # else branding!="Chrome"
      'variables': {
         'branding_dir': '../app/theme/chromium',
      },
    }],
  ],
}
