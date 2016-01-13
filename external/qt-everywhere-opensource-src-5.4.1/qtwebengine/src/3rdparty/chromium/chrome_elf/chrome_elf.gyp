# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/util/version.gypi',
    '../build/win_precompile.gypi',
    'blacklist.gypi',
    'dll_hash.gypi',
  ],
  'targets': [
    {
      'target_name': 'chrome_elf_resources',
      'type': 'none',
      'conditions': [
        ['branding == "Chrome"', {
          'variables': {
             'branding_path': '../chrome/app/theme/google_chrome/BRANDING',
          },
        }, { # else branding!="Chrome"
          'variables': {
             'branding_path': '../chrome/app/theme/chromium/BRANDING',
          },
        }],
      ],
      'variables': {
        'output_dir': 'chrome_elf',
        'template_input_path': '../chrome/app/chrome_version.rc.version',
      },
      'sources': [
        'chrome_elf.ver',
      ],
      'includes': [
        '../chrome/version_resource_rules.gypi',
      ],
    },
    {
      'target_name': 'chrome_elf',
      'type': 'shared_library',
      'include_dirs': [
        '..',
      ],
      'sources': [
        'chrome_elf.def',
        'chrome_elf_main.cc',
        'chrome_elf_main.h',
        '<(SHARED_INTERMEDIATE_DIR)/chrome_elf/chrome_elf_version.rc',
      ],
      'dependencies': [
        'blacklist',
        'chrome_elf_breakpad',
        'chrome_elf_lib',
        'chrome_elf_resources',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'conditions': [
            ['target_arch=="ia32"', {
              # Don't set an x64 base address (to avoid breaking HE-ASLR).
              'BaseAddress': '0x01c20000',
            }],
          ],
          # Set /SUBSYSTEM:WINDOWS.
          'SubSystem': '2',
          'AdditionalDependencies!': [
            'user32.lib',
          ],
          'IgnoreDefaultLibraryNames': [
            'user32.lib',
          ],
        },
      },
    },
    {
      'target_name': 'chrome_elf_unittests_exe',
      'product_name': 'chrome_elf_unittests',
      'type': 'executable',
      'sources': [
        'blacklist/test/blacklist_test.cc',
        'chrome_elf_util_unittest.cc',
        'create_file/chrome_create_file_unittest.cc',
        'elf_imports_unittest.cc',
        'ntdll_cache_unittest.cc',
      ],
      'include_dirs': [
        '..',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'dependencies': [
        'chrome_elf_lib',
        '../base/base.gyp:base',
        '../base/base.gyp:run_all_unittests',
        '../base/base.gyp:test_support_base',
        '../sandbox/sandbox.gyp:sandbox',
        '../testing/gtest.gyp:gtest',
        'blacklist',
        'blacklist_test_dll_1',
        'blacklist_test_dll_2',
        'blacklist_test_dll_3',
        'blacklist_test_main_dll',
      ],
    },
    {
      # A dummy target to ensure that chrome_elf.dll and chrome.exe gets built
      # when building chrome_elf_unittests.exe without introducing an
      # explicit runtime dependency.
      'target_name': 'chrome_elf_unittests',
      'type': 'none',
      'dependencies': [
        '../chrome/chrome.gyp:chrome',
        'chrome_elf',
        'chrome_elf_unittests_exe',
      ],
    },
    {
      'target_name': 'chrome_elf_lib',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'sources': [
        'create_file/chrome_create_file.cc',
        'create_file/chrome_create_file.h',
        'ntdll_cache.cc',
        'ntdll_cache.h',
      ],
      'dependencies': [
        'chrome_elf_common',
        '../base/base.gyp:base_static',
        '../sandbox/sandbox.gyp:sandbox',
      ],
    },
    {
      'target_name': 'chrome_elf_constants',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'sources': [
        'chrome_elf_constants.cc',
        'chrome_elf_constants.h',
      ],
    },
    {
      'target_name': 'chrome_elf_common',
      'type': 'static_library',
      'dependencies': [
        'chrome_elf_constants',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'chrome_elf_types.h',
        'chrome_elf_util.cc',
        'chrome_elf_util.h',
        'thunk_getter.cc',
        'thunk_getter.h',
      ],
    },
    {
      'target_name': 'chrome_elf_breakpad',
      'type': 'static_library',
      'include_dirs': [
        '..',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'sources': [
        'breakpad.cc',
        'breakpad.h',
      ],
      'dependencies': [
        'chrome_elf_common',
        '../breakpad/breakpad.gyp:breakpad_handler',
        '../chrome/chrome.gyp:chrome_version_header',
      ],
    },
  ], # targets
  'conditions': [
    ['component=="shared_library"', {
      'targets': [
        {
          'target_name': 'chrome_redirects',
          'type': 'shared_library',
          'include_dirs': [
            '..',
          ],
          'sources': [
            'chrome_redirects.def',
            'chrome_redirects_main.cc',
          ],
          'dependencies': [
            'chrome_elf_lib',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'conditions': [
                ['target_arch=="ia32"', {
                  # Don't set an x64 base address (to avoid breaking HE-ASLR).
                  'BaseAddress': '0x01c20000',
                }],
              ],
              # Set /SUBSYSTEM:WINDOWS.
              'SubSystem': '2',
            },
          },
        },
      ],
    }],
  ],
}

