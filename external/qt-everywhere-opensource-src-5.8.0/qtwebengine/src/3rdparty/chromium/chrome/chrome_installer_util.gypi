# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'installer_util_target': 0,
    },
    'target_conditions': [
      # This part is shared between the two versions of the target.
      ['installer_util_target==1', {
        'sources': [
          'installer/util/advanced_firewall_manager_win.cc',
          'installer/util/advanced_firewall_manager_win.h',
          'installer/util/app_command.cc',
          'installer/util/app_command.h',
          'installer/util/app_commands.cc',
          'installer/util/app_commands.h',
          'installer/util/app_registration_data.h',
          'installer/util/auto_launch_util.cc',
          'installer/util/auto_launch_util.h',
          'installer/util/beacons.cc',
          'installer/util/beacons.h',
          'installer/util/browser_distribution.cc',
          'installer/util/browser_distribution.h',
          'installer/util/callback_work_item.cc',
          'installer/util/callback_work_item.h',
          'installer/util/channel_info.cc',
          'installer/util/channel_info.h',
          'installer/util/chrome_frame_distribution.cc',
          'installer/util/chrome_frame_distribution.h',
          'installer/util/chromium_binaries_distribution.cc',
          'installer/util/chromium_binaries_distribution.h',
          'installer/util/conditional_work_item_list.cc',
          'installer/util/conditional_work_item_list.h',
          'installer/util/copy_tree_work_item.cc',
          'installer/util/copy_tree_work_item.h',
          'installer/util/create_dir_work_item.cc',
          'installer/util/create_dir_work_item.h',
          'installer/util/create_reg_key_work_item.cc',
          'installer/util/create_reg_key_work_item.h',
          'installer/util/delete_reg_key_work_item.cc',
          'installer/util/delete_reg_key_work_item.h',
          'installer/util/delete_reg_value_work_item.cc',
          'installer/util/delete_reg_value_work_item.h',
          'installer/util/delete_tree_work_item.cc',
          'installer/util/delete_tree_work_item.h',
          'installer/util/duplicate_tree_detector.cc',
          'installer/util/duplicate_tree_detector.h',
          'installer/util/firewall_manager_win.cc',
          'installer/util/firewall_manager_win.h',
          'installer/util/google_chrome_binaries_distribution.cc',
          'installer/util/google_chrome_binaries_distribution.h',
          'installer/util/google_chrome_sxs_distribution.cc',
          'installer/util/google_chrome_sxs_distribution.h',
          'installer/util/google_update_constants.cc',
          'installer/util/google_update_constants.h',
          'installer/util/google_update_settings.cc',
          'installer/util/google_update_settings.h',
          'installer/util/google_update_util.cc',
          'installer/util/google_update_util.h',
          'installer/util/helper.cc',
          'installer/util/helper.h',
          'installer/util/install_util.cc',
          'installer/util/install_util.h',
          'installer/util/installation_state.cc',
          'installer/util/installation_state.h',
          'installer/util/installer_state.cc',
          'installer/util/installer_state.h',
          'installer/util/l10n_string_util.cc',
          'installer/util/l10n_string_util.h',
          'installer/util/language_selector.cc',
          'installer/util/language_selector.h',
          'installer/util/legacy_firewall_manager_win.cc',
          'installer/util/legacy_firewall_manager_win.h',
          'installer/util/master_preferences_constants.cc',
          'installer/util/master_preferences_constants.h',
          'installer/util/module_util_win.cc',
          'installer/util/module_util_win.h',
          'installer/util/move_tree_work_item.cc',
          'installer/util/move_tree_work_item.h',
          'installer/util/non_updating_app_registration_data.cc',
          'installer/util/non_updating_app_registration_data.h',
          'installer/util/registry_key_backup.cc',
          'installer/util/registry_key_backup.h',
          'installer/util/self_reg_work_item.cc',
          'installer/util/self_reg_work_item.h',
          'installer/util/set_reg_value_work_item.cc',
          'installer/util/set_reg_value_work_item.h',
          'installer/util/updating_app_registration_data.cc',
          'installer/util/updating_app_registration_data.h',
          'installer/util/util_constants.cc',
          'installer/util/util_constants.h',
          'installer/util/wmi.cc',
          'installer/util/wmi.h',
          'installer/util/work_item.cc',
          'installer/util/work_item.h',
          'installer/util/work_item_list.cc',
          'installer/util/work_item_list.h',
        ],
        'include_dirs': [
          '<(DEPTH)',
        ],
      }],
    ],
  },
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          # GN version: //chrome/installer/util
          'target_name': 'installer_util',
          'type': 'static_library',
          'variables': {
            'installer_util_target': 1,
          },
          'dependencies': [
            'installer_util_strings',
            '<(DEPTH)/base/base.gyp:base',
            '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
            '<(DEPTH)/chrome/chrome_resources.gyp:chrome_strings',
            '<(DEPTH)/chrome/common_constants.gyp:common_constants',
            '<(DEPTH)/components/components.gyp:base32',
            '<(DEPTH)/components/components.gyp:metrics',
            '<(DEPTH)/components/components.gyp:variations',
            '<(DEPTH)/courgette/courgette.gyp:courgette_lib',
            '<(DEPTH)/crypto/crypto.gyp:crypto',
            '<(DEPTH)/third_party/bspatch/bspatch.gyp:bspatch',
            '<(DEPTH)/third_party/crashpad/crashpad/client/client.gyp:crashpad_client',
            '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
            '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
            '<(DEPTH)/third_party/lzma_sdk/lzma_sdk.gyp:lzma_sdk',
          ],
          'sources': [
            'installer/util/chrome_binaries_operations.cc',
            'installer/util/chrome_binaries_operations.h',
            'installer/util/chrome_browser_operations.cc',
            'installer/util/chrome_browser_operations.h',
            'installer/util/chrome_browser_sxs_operations.cc',
            'installer/util/chrome_browser_sxs_operations.h',
            'installer/util/chrome_frame_operations.cc',
            'installer/util/chrome_frame_operations.h',
            'installer/util/compat_checks.cc',
            'installer/util/compat_checks.h',
            'installer/util/delete_after_reboot_helper.cc',
            'installer/util/delete_after_reboot_helper.h',
            'installer/util/google_chrome_distribution.cc',
            'installer/util/google_chrome_distribution.h',
            'installer/util/html_dialog.h',
            'installer/util/html_dialog_impl.cc',
            'installer/util/installation_validator.cc',
            'installer/util/installation_validator.h',
            'installer/util/logging_installer.cc',
            'installer/util/logging_installer.h',
            'installer/util/lzma_util.cc',
            'installer/util/lzma_util.h',
            'installer/util/lzma_file_allocator.cc',
            'installer/util/lzma_file_allocator.h',
            'installer/util/master_preferences.cc',
            'installer/util/master_preferences.h',
            'installer/util/product.cc',
            'installer/util/product.h',
            'installer/util/product_operations.h',
            'installer/util/registry_entry.cc',
            'installer/util/registry_entry.h',
            "installer/util/scoped_user_protocol_entry.cc",
            "installer/util/scoped_user_protocol_entry.h",
            'installer/util/self_cleaning_temp_dir.cc',
            'installer/util/self_cleaning_temp_dir.h',
            'installer/util/shell_util.cc',
            'installer/util/shell_util.h',
            'installer/util/uninstall_metrics.cc',
            'installer/util/uninstall_metrics.h',
            'installer/util/user_experiment.cc',
            'installer/util/user_experiment.h',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
          'all_dependent_settings': {
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalDependencies': [
                  'urlmon.lib',
                  'wbemuuid.lib',
                  'wtsapi32.lib',
                ],
              },
            },
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalDependencies': [
                'urlmon.lib',
                'wbemuuid.lib',
                'wtsapi32.lib',
              ],
            },
          },
        },
      ],
    }],
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'installer_util_nacl_win64',
          'type': 'static_library',
          'variables': {
            'installer_util_target': 1,
          },
          'dependencies': [
            'installer_util_strings',
          ],
          'include_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)',
          ],
          'sources': [
            # Include |client_info.cc| directly here to avoid having to create a
            # metrics_win64 target solely for this purpose.
            '../components/metrics/client_info.cc',
            '../components/metrics/client_info.h',
            'installer/util/google_chrome_distribution_dummy.cc',
            'installer/util/master_preferences.h',
            'installer/util/master_preferences_dummy.cc',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
          'all_dependent_settings': {
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalDependencies': [
                  'wbemuuid.lib',
                ],
              },
            },
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalDependencies': [
                'wbemuuid.lib',
              ],
            },
          },
        },
      ],
    }],
    ['OS!="win"', {
      'targets': [
        {
          # GN version: //chrome/installer/util
          'target_name': 'installer_util',
          'type': 'static_library',
          'dependencies': [
            '<(DEPTH)/base/base.gyp:base',
            '<(DEPTH)/chrome/chrome_resources.gyp:chrome_resources',
            '<(DEPTH)/chrome/chrome_resources.gyp:chrome_strings',
            '<(DEPTH)/components/components.gyp:variations',
          ],
          'sources': [
            # Note: sources list duplicated in GN build.
            'installer/util/master_preferences.cc',
            'installer/util/master_preferences.h',
            'installer/util/master_preferences_constants.cc',
            'installer/util/master_preferences_constants.h',
          ],
          'include_dirs': [
            '<(DEPTH)',
          ],
        }
      ],
    }],

  ],
}
