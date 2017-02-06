{
  'variables': {
    'chromium_code': 1,
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
          'variables': {
            'chrome_dll_project': [
              '../chrome.gyp:chrome_dll',
            ],
            'chrome_dll_path': [
              '<(PRODUCT_DIR)/chrome.dll',
            ],
            'output_dir': '<(PRODUCT_DIR)',
          },
          'includes': [
            'mini_installer.gypi',
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
              ],
              'sources': [
                'mini_installer_tests.isolate',
              ],
            },
          ],
        }],
        # next_version_mini_installer.exe can't be generated in an x86 Debug
        # component build because it requires too much memory. Don't define the
        # target for any x86 component build since gyp doesn't allow use of the
        # configuration name in conditionals.
        ['component!="shared_library" or target_arch!="ia32"', {
          'targets': [
            {
              # GN version: //chrome/installer/mini_installer:next_version_mini_installer
              'target_name': 'next_version_mini_installer',
              'type': 'none',
              'dependencies': [
                'mini_installer',
                '<(DEPTH)/chrome/installer/upgrade_test.gyp:alternate_version_generator',
              ],
              'variables': {
                'alternate_version_generator_exe': 'alternate_version_generator.exe',
                'next_version_mini_installer_exe': 'next_version_mini_installer.exe',
              },
              'actions': [
                {
                  'action_name': 'generate_next_version_mini_installer',
                  'inputs': [
                    '<(PRODUCT_DIR)/<(alternate_version_generator_exe)',
                    '<(PRODUCT_DIR)/mini_installer.exe',
                  ],
                  'outputs': [
                    '<(PRODUCT_DIR)/next_version_mini_installer.exe',
                  ],
                  'action': [
                    '<(PRODUCT_DIR)/<(alternate_version_generator_exe)',
                    '--force',
                    '--out=<(PRODUCT_DIR)/<(next_version_mini_installer_exe)',
                  ],
                }
              ],
            },
          ],
        }],
      ],
    }],
  ],
}
