# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# this file is meant to be included within a target dict to generate the
# mini_installer, the embedder must provide the following variables:
#   - chrome_dll_project: The target generating the main chrome DLLs.
#   - chrome_dll_path: The path to the version of chrome.dll to put in the
#       mini_installer.
#   - The output directory for mini_installer.exe.
{
  'type': 'executable',
  'dependencies': [
    '<@(chrome_dll_project)',
    '../chrome.gyp:chrome',
    '../chrome.gyp:chrome_nacl_win64',
    '../chrome.gyp:default_extensions',
    '../chrome.gyp:setup',
    'installer_tools.gyp:test_installer_sentinel',
    'mini_installer_version.gyp:mini_installer_version',
  ],
  'include_dirs': [
    '../..',
    '<(INTERMEDIATE_DIR)',
    '<(SHARED_INTERMEDIATE_DIR)/chrome',
  ],
  'sources': [
    '<(INTERMEDIATE_DIR)/packed_files.rc',
    '<(PRODUCT_DIR)/mini_installer_exe_version.rc',
    'mini_installer/appid.h',
    'mini_installer/chrome.release',
    'mini_installer/chrome_appid.cc',
    'mini_installer/configuration.cc',
    'mini_installer/configuration.h',
    'mini_installer/decompress.cc',
    'mini_installer/decompress.h',
    'mini_installer/exit_code.h',
    'mini_installer/mini_installer.cc',
    'mini_installer/mini_installer.ico',
    'mini_installer/mini_installer.rc',
    'mini_installer/mini_installer_constants.cc',
    'mini_installer/mini_installer_constants.h',
    'mini_installer/mini_installer_exe_version.rc.version',
    'mini_installer/mini_installer_resource.h',
    'mini_installer/mini_string.cc',
    'mini_installer/mini_string.h',
    'mini_installer/pe_resource.cc',
    'mini_installer/pe_resource.h',
    'mini_installer/regkey.cc',
    'mini_installer/regkey.h',
  ],
  'msvs_settings': {
    'VCCLCompilerTool': {
      'EnableIntrinsicFunctions': 'true',
      'BufferSecurityCheck': 'false',
      'BasicRuntimeChecks': '0',
      'ExceptionHandling': '0',
    },
    'VCLinkerTool': {
      'OutputFile': '<(output_dir)/mini_installer.exe',
      'RandomizedBaseAddress': '1',
      'DataExecutionPrevention': '0',
      'AdditionalLibraryDirectories': [
        '<(PRODUCT_DIR)/lib'
      ],
      'DelayLoadDLLs': [],
      'EntryPointSymbol': 'MainEntryPoint',
      'IgnoreAllDefaultLibraries': 'true',
      'OptimizeForWindows98': '1',
      'SubSystem': '2',  # Set /SUBSYSTEM:WINDOWS
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
          'WholeProgramOptimization': 'false',
        },
        'VCLinkerTool': {
          'SubSystem': '2',     # Set /SUBSYSTEM:WINDOWS
          'Profile': 'false',  # Conflicts with /FIXED
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
  'actions': [
    {
      'action_name': 'installer_archive',
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
            '<(output_dir)/chrome.packed.7z',
          ],
        }],
        ['disable_nacl==1', {
          'inputs!': [
            '<(PRODUCT_DIR)/nacl64.exe',
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
        ['v8_use_external_startup_data == 1', {
          'inputs': [
            '<(PRODUCT_DIR)/natives_blob.bin',
            '<(PRODUCT_DIR)/snapshot_blob.bin',
          ],
        }],
      ],
      'inputs': [
        '<(create_installer_archive_py_path)',
        '<(PRODUCT_DIR)/chrome.exe',
        '<@(chrome_dll_path)',
        '<(PRODUCT_DIR)/nacl64.exe',
        '<(PRODUCT_DIR)/nacl_irt_x86_32.nexe',
        '<(PRODUCT_DIR)/nacl_irt_x86_64.nexe',
        '<(PRODUCT_DIR)/locales/en-US.pak',
        '<(PRODUCT_DIR)/setup.exe',
        'mini_installer/chrome.release',
      ],
      'outputs': [
        # Also note that chrome.packed.7z is defined as an output in a
        # conditional above.
        '<(output_dir)/chrome.7z',
        '<(output_dir)/setup.ex_',
        '<(INTERMEDIATE_DIR)/packed_files.rc',
      ],
      'depfile': '<(INTERMEDIATE_DIR)/installer_archive.d',
      'action': [
        'python',
        '<(create_installer_archive_py_path)',
        '--build_dir=<(PRODUCT_DIR)',
        '--output_dir=<(output_dir)',
        '--staging_dir=<(INTERMEDIATE_DIR)',
        '--input_file=mini_installer/chrome.release',
        '--resource_file_path=<(INTERMEDIATE_DIR)/packed_files.rc',
        '--depfile=<(INTERMEDIATE_DIR)/installer_archive.d',
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
        # Optional argument for verbose archiving output.
        #'--verbose',
      ],
      'message': 'Create installer archive',
    },
  ],
  'conditions': [
    ['OS=="win" and buildtype=="Official"', {
      # Optimize for size when doing an official build.
      'optimize' :'size',
    }],
  ],
}
