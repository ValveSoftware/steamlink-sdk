# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'dependencies': [
    '<@(chrome_dll_project)',
    '../chrome.gyp:chrome',
    '../chrome.gyp:chrome_nacl_win64',
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
  # Disable precompiled headers for this project, to avoid
  # linker errors when building with VS 2008.
  'msvs_precompiled_header': '',
  'msvs_precompiled_source': '',
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
          'SubSystem': '2',  # Set /SUBSYSTEM:WINDOWS
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
          'SubSystem': '2',  # Set /SUBSYSTEM:WINDOWS
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
  'rules': [
    {
      'rule_name': 'mini_installer_version',
      'extension': 'version',
      'variables': {
        'template_input_path':
            'mini_installer/mini_installer_exe_version.rc.version',
      },
      'inputs': [
        '<(template_input_path)',
        '<(version_path)',
        '<(lastchange_path)',
        '<(branding_dir)/BRANDING',
      ],
      'outputs': [
        '<(INTERMEDIATE_DIR)/mini_installer_exe_version.rc',
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
        '<@(chrome_dll_path)',
        '<(PRODUCT_DIR)/nacl64.exe',
        '<(PRODUCT_DIR)/ppGoogleNaClPluginChrome.dll',
        '<(PRODUCT_DIR)/nacl_irt_x86_32.nexe',
        '<(PRODUCT_DIR)/nacl_irt_x86_64.nexe',
        '<(PRODUCT_DIR)/locales/en-US.pak',
      ],
      'outputs': [
        'xxx.out',
        '<(output_dir)/<(RULE_INPUT_NAME).7z',
        '<(output_dir)/<(RULE_INPUT_NAME).packed.7z',
        '<(output_dir)/setup.ex_',
        '<(INTERMEDIATE_DIR)/packed_files.rc',
      ],
      'action': [
        'python',
        '<(create_installer_archive_py_path)',
        '--build_dir=<(PRODUCT_DIR)',
        '--output_dir=<(output_dir)',
        '--staging_dir=<(INTERMEDIATE_DIR)',
        '--input_file=<(RULE_INPUT_PATH)',
        '--resource_file_path=<(INTERMEDIATE_DIR)/packed_files.rc',
        '<(enable_hidpi_flag)',
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
}
