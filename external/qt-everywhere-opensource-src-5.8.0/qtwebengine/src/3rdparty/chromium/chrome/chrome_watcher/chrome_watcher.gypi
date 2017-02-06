# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../../build/util/version.gypi',
    '../../build/win_precompile.gypi',
  ],
  'targets': [
    {
      'target_name': 'kasko_util',
      'type': 'none',
      'dependencies': [
        '../third_party/kasko/kasko.gyp:kasko_features',
      ],
    },
    {
      'target_name': 'hang_util',
      'type': 'static_library',
      'sources': [
        'system_load_estimator.cc',
        'system_load_estimator.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../components/components.gyp:memory_pressure'
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'DelayLoadDLLs': [
            'pdh.dll',  # Used by system_load_estimator.h on report capture.
          ],
          'AdditionalDependencies': [
            'pdh.lib',
          ],
        },
      },
      'all_dependent_settings': {
        'msvs_settings': {
          'VCLinkerTool': {
            'DelayLoadDLLs': [
              # Used by system_load_estimator.h on report capture.
              'pdh.dll',
            ],
            'AdditionalDependencies': [
              'pdh.lib',
            ],
          },
        },
      },
    },
    {
      # GN version: //chrome/chrome_watcher:system_load_estimator_unittests
      'target_name': 'system_load_estimator_unittests',
      'type': '<(gtest_target_type)',
      'sources': [
        'system_load_estimator_unittest.cc',
      ],
      'dependencies': [
        'hang_util',
        '../base/base.gyp:base',
        '../base/base.gyp:run_all_unittests',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
      ],
    },
    {
      'target_name': 'chrome_watcher_resources',
      'type': 'none',
      'conditions': [
        ['branding == "Chrome"', {
          'variables': {
             'branding_path': '../app/theme/google_chrome/BRANDING',
          },
        }, { # else branding!="Chrome"
          'variables': {
             'branding_path': '../app/theme/chromium/BRANDING',
          },
        }],
      ],
      'variables': {
        'output_dir': '.',
        'template_input_path': '../app/chrome_version.rc.version',
      },
      'sources': [
        'chrome_watcher.ver',
      ],
      'includes': [
        '../version_resource_rules.gypi',
      ],
    },
    {
      # Users of the watcher link this target.
      'target_name': 'chrome_watcher_client',
      'type': 'static_library',
      'sources': [
        'chrome_watcher_main_api.cc',
        'chrome_watcher_main_api.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'chrome_watcher',
      'type': 'loadable_module',
      'include_dirs': [
        '../..',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/chrome_watcher/chrome_watcher_version.rc',
        'chrome_watcher.def',
        'chrome_watcher_main.cc',
      ],
      'dependencies': [
        'chrome_watcher_client',
        'chrome_watcher_resources',
        'kasko_util',
        'installer_util',
        '../base/base.gyp:base',
        '../components/components.gyp:browser_watcher',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          # Set /SUBSYSTEM:WINDOWS.
          'SubSystem': '2',
        },
      },
    },
  ],
}
