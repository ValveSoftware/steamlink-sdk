# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../media/cdm_paths.gypi',
  ],
  'variables': {
    'lastchange_path': '../build/util/LASTCHANGE',
    'branding_dir': 'app/theme/<(branding_path_component)',
    'branding_dir_100': 'app/theme/default_100_percent/<(branding_path_component)',
  },
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          # GN version: //chrome/installer/gcapi
          'target_name': 'gcapi_dll',
          'type': 'loadable_module',
          'dependencies': [
            'gcapi_lib',
            '../chrome/chrome.gyp:install_static_util',
            '../chrome/common_constants.gyp:version_header',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'installer/gcapi/gcapi.def',
            'installer/gcapi/gcapi_dll.cc',
            'installer/gcapi/gcapi_dll_version.rc.version',
          ],
          'copies': [{
            'destination': '<(PRODUCT_DIR)',
            'files': [
              'installer/gcapi/gcapi.h',
            ],
          }],
          'rules': [
            {
              'rule_name': 'gcapi_version',
              'extension': 'version',
              'variables': {
                'version_py_path': '<(DEPTH)/build/util/version.py',
                'template_input_path': 'installer/gcapi/gcapi_dll_version.rc.version',
              },
              'inputs': [
                '<(template_input_path)',
                '<(version_path)',
                '<(lastchange_path)',
                '<(branding_dir)/BRANDING',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/gcapi/gcapi_dll_version.rc',
              ],
              'action': [
                'python', '<(version_py_path)',
                '-f', '<(version_path)',
                '-f', '<(lastchange_path)',
                '-f', '<(branding_dir)/BRANDING',
                '<(template_input_path)',
                '<@(_outputs)',
              ],
              'process_outputs_as_sources': 1,
              'message': 'Generating version information'
            },
          ],
        },
        {
          # GN version: //chrome/installer/gcapi:lib
          'target_name': 'gcapi_lib',
          'type': 'static_library',
          'dependencies': [
            'installer_util',
            '../base/base.gyp:base',
            '../chrome/chrome.gyp:launcher_support',
            '../components/components.gyp:variations',
            '../google_update/google_update.gyp:google_update',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'installer/gcapi/gcapi.cc',
            'installer/gcapi/gcapi.h',
            'installer/gcapi/gcapi_omaha_experiment.cc',
            'installer/gcapi/gcapi_omaha_experiment.h',
            'installer/gcapi/gcapi_reactivation.cc',
            'installer/gcapi/gcapi_reactivation.h',
          ],
        },
        {
          # GN version: //chrome/installer/gcapi:gcapi_test
          'target_name': 'gcapi_test',
          'type': 'executable',
          'dependencies': [
            'common',
            'gcapi_dll',
            'gcapi_lib',
            'installer_util',
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
            '../chrome/chrome.gyp:install_static_util',
            '../components/components.gyp:variations',
            '../testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'installer/gcapi/gcapi_last_run_test.cc',
            'installer/gcapi/gcapi_omaha_experiment_test.cc',
            'installer/gcapi/gcapi_reactivation_test.cc',
            'installer/gcapi/gcapi_test.cc',
            'installer/gcapi/gcapi_test.rc',
            'installer/gcapi/gcapi_test_registry_overrider.cc',
            'installer/gcapi/gcapi_test_registry_overrider.h',
            'installer/gcapi/resource.h',
          ],
        },
        {
          # GN version: //chrome/installer/util:installer_util_unittests
          'target_name': 'installer_util_unittests',
          'type': 'executable',
          'dependencies': [
            'installer_util',
            'installer_util_strings',
            'installer/upgrade_test.gyp:alternate_version_generator_lib',
            '../base/base.gyp:base',
            '../base/base.gyp:base_i18n',
            '../base/base.gyp:test_support_base',
            '../chrome/chrome.gyp:chrome_version_resources',
            '../chrome/chrome.gyp:install_static_util',
            '../components/components.gyp:variations',
            '../content/content.gyp:content_common',
            '../testing/gmock.gyp:gmock',
            '../testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            # List duplicated in GN build.
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/installer/util/installer_util_strings.rc',
            'installer/setup/compat_checks_unittest.cc',
            'installer/setup/setup_constants.cc',
            'installer/util/advanced_firewall_manager_win_unittest.cc',
            'installer/util/beacons_unittest.cc',
            'installer/util/callback_work_item_unittest.cc',
            'installer/util/channel_info_unittest.cc',
            'installer/util/conditional_work_item_list_unittest.cc',
            'installer/util/copy_tree_work_item_unittest.cc',
            'installer/util/create_dir_work_item_unittest.cc',
            'installer/util/create_reg_key_work_item_unittest.cc',
            'installer/util/delete_after_reboot_helper_unittest.cc',
            'installer/util/delete_reg_key_work_item_unittest.cc',
            'installer/util/delete_reg_value_work_item_unittest.cc',
            'installer/util/delete_tree_work_item_unittest.cc',
            'installer/util/duplicate_tree_detector_unittest.cc',
            'installer/util/fake_installation_state.h',
            'installer/util/fake_product_state.h',
            'installer/util/google_update_settings_unittest.cc',
            'installer/util/install_util_unittest.cc',
            'installer/util/installation_validation_helper.cc',
            'installer/util/installation_validation_helper.h',
            'installer/util/installation_validator_unittest.cc',
            'installer/util/installer_state_unittest.cc',
            'installer/util/installer_util_test_common.cc',
            'installer/util/installer_util_test_common.h',
            'installer/util/language_selector_unittest.cc',
            'installer/util/legacy_firewall_manager_win_unittest.cc',
            'installer/util/logging_installer_unittest.cc',
            'installer/util/lzma_file_allocator_unittest.cc',
            'installer/util/lzma_util_unittest.cc',
            'installer/util/master_preferences_unittest.cc',
            'installer/util/move_tree_work_item_unittest.cc',
            'installer/util/product_state_unittest.cc',
            'installer/util/product_unittest.cc',
            'installer/util/registry_key_backup_unittest.cc',
            'installer/util/registry_test_data.cc',
            'installer/util/registry_test_data.h',
            'installer/util/run_all_unittests.cc',
            "installer/util/scoped_user_protocol_entry_unittest.cc",
            'installer/util/self_cleaning_temp_dir_unittest.cc',
            'installer/util/set_reg_value_work_item_unittest.cc',
            'installer/util/shell_util_unittest.cc',
            'installer/util/test_app_registration_data.cc',
            'installer/util/test_app_registration_data.h',
            'installer/util/uninstall_metrics_unittest.cc',
            'installer/util/wmi_unittest.cc',
            'installer/util/work_item_list_unittest.cc',
            'installer/util/work_item_mocks.cc',
            'installer/util/work_item_mocks.h',
            'installer/util/work_item_unittest.cc',
          ],
          'msvs_settings': {
            'VCManifestTool': {
              'AdditionalManifestFiles': [
                '$(ProjectDir)\\installer\\mini_installer\\mini_installer.exe.manifest',
              ],
            },
          },
        },
        {
          # GN version: //chrome/installer/util:strings
          'target_name': 'installer_util_strings',
          'type': 'none',
          'actions': [
            {
              'action_name': 'installer_util_strings',
              'variables': {
                'create_string_rc_py': 'installer/util/prebuild/create_string_rc.py',
                'brand_strings': '<(branding_path_component)_strings',
                'gen_dir': '<(SHARED_INTERMEDIATE_DIR)/chrome/installer/util',
              },

              'inputs': [
                '<(create_string_rc_py)',
                'app/<(brand_strings).grd',
              ],
              'outputs': [
                '<(gen_dir)/installer_util_strings.h',
                '<(gen_dir)/installer_util_strings.rc',
              ],
              'action': ['python',
                         '<(create_string_rc_py)',
                         '-i', 'app/<(brand_strings).grd:resources',
                         '-n', 'installer_util_strings',
                         '-o', '<(gen_dir)',],
              'message': 'Generating installer_util_strings',
            },
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)',
            ],
          },
        },
        {
          # GN version: //chrome/installer/launcher_support
          'target_name': 'launcher_support',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '..',
            ],
          },
          'dependencies': [
            '<(DEPTH)/base/base.gyp:base',
          ],
          'sources': [
            'installer/launcher_support/chrome_launcher_support.cc',
            'installer/launcher_support/chrome_launcher_support.h',
          ],
        },
        {
          # GN version: //chrome/installer/setup:lib
          'target_name': 'setup_lib',
          'type': 'static_library',
          'dependencies': [
            'installer_util',
            'installer_util_strings',
            '../base/base.gyp:base',
            '../chrome/common_constants.gyp:version_header',
            '../components/components.gyp:crash_component',
          ],
          'include_dirs': [
            '..',
            '<(INTERMEDIATE_DIR)',
          ],
          'sources': [
            'installer/setup/app_launcher_installer.cc',
            'installer/setup/app_launcher_installer.h',
            'installer/setup/archive_patch_helper.cc',
            'installer/setup/archive_patch_helper.h',
            'installer/setup/install.cc',
            'installer/setup/install.h',
            'installer/setup/install_worker.cc',
            'installer/setup/install_worker.h',
            'installer/setup/installer_crash_reporter_client.cc',
            'installer/setup/installer_crash_reporter_client.h',
            'installer/setup/installer_crash_reporting.cc',
            'installer/setup/installer_crash_reporting.h',
            'installer/setup/installer_metrics.cc',
            'installer/setup/installer_metrics.h',
            'installer/setup/setup_constants.cc',
            'installer/setup/setup_constants.h',
            'installer/setup/setup_util.cc',
            'installer/setup/setup_util.h',
            'installer/setup/update_active_setup_version_work_item.cc',
            'installer/setup/update_active_setup_version_work_item.h',
            'installer/setup/user_hive_visitor.cc',
            'installer/setup/user_hive_visitor.h',
          ],
        },
        {
          # GN version: //chrome/installer/setup
          'target_name': 'setup',
          'type': 'executable',
          'dependencies': [
            'setup_lib',
            '../chrome/chrome.gyp:install_static_util',
            '../chrome/common_constants.gyp:common_constants',
            '../chrome/common_constants.gyp:version_header',
            '../chrome_elf/chrome_elf.gyp:chrome_elf_constants',
            '../components/components.gyp:base32',
            '../components/components.gyp:crash_component',
            '../rlz/rlz.gyp:rlz_lib',
            '../third_party/zlib/zlib.gyp:zlib',
          ],
          'defines': [
            'COMPILE_CONTENT_STATICALLY',
          ],
          'include_dirs': [
            '..',
            '<(INTERMEDIATE_DIR)',
            '<(SHARED_INTERMEDIATE_DIR)/setup',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/installer/util/installer_util_strings.rc',
            '../content/public/common/content_switches.cc',
            'installer/setup/setup.ico',
            'installer/setup/setup.rc',
            'installer/setup/setup_exe_version.rc.version',
            'installer/setup/setup_main.cc',
            'installer/setup/setup_main.h',
            'installer/setup/setup_resource.h',
            'installer/setup/uninstall.cc',
            'installer/setup/uninstall.h',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'SubSystem': '2',     # Set /SUBSYSTEM:WINDOWS
            },
            'VCManifestTool': {
              'AdditionalManifestFiles': [
                '$(ProjectDir)\\installer\\setup\\setup.exe.manifest',
              ],
            },
          },
          'rules': [
            {
              'rule_name': 'setup_version',
              'extension': 'version',
              'variables': {
                'version_py_path': '<(DEPTH)/build/util/version.py',
                'template_input_path': 'installer/setup/setup_exe_version.rc.version',
              },
              'inputs': [
                '<(template_input_path)',
                '<(version_path)',
                '<(lastchange_path)',
                '<(branding_dir)/BRANDING',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/setup/setup_exe_version.rc',
              ],
              'action': [
                'python', '<(version_py_path)',
                '-f', '<(version_path)',
                '-f', '<(lastchange_path)',
                '-f', '<(branding_dir)/BRANDING',
                '<(template_input_path)',
                '<@(_outputs)',
              ],
              'process_outputs_as_sources': 1,
              'message': 'Generating version information'
            },
          ],
          'conditions': [
            ['target_arch=="ia32"', {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'EnableEnhancedInstructionSet': '4',  # NoExtensions
                },
              },
            }],
          ],
        },
        {
          # GN version: //chrome/installer/setup:setup_unittests
          'target_name': 'setup_unittests',
          'type': 'executable',
          'dependencies': [
            'setup_lib',
            '../base/base.gyp:base_i18n',
            '../base/base.gyp:test_support_base',
            '../chrome/chrome.gyp:install_static_util',
            '../testing/gmock.gyp:gmock',
            '../testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '..',
            '<(INTERMEDIATE_DIR)',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/installer/util/installer_util_strings.rc',
            'installer/mini_installer/appid.h',
            'installer/mini_installer/chrome_appid.cc',
            'installer/mini_installer/configuration.cc',
            'installer/mini_installer/configuration.h',
            'installer/mini_installer/configuration_test.cc',
            'installer/mini_installer/decompress.cc',
            'installer/mini_installer/decompress.h',
            'installer/mini_installer/decompress_test.cc',
            'installer/mini_installer/mini_installer_constants.cc',
            'installer/mini_installer/mini_installer_constants.h',
            'installer/mini_installer/mini_string.cc',
            'installer/mini_installer/mini_string.h',
            'installer/mini_installer/mini_string_test.cc',
            'installer/mini_installer/regkey.cc',
            'installer/mini_installer/regkey.h',
            'installer/setup/archive_patch_helper_unittest.cc',
            'installer/setup/install_unittest.cc',
            'installer/setup/install_worker_unittest.cc',
            'installer/setup/memory_unittest.cc',
            'installer/setup/run_all_unittests.cc',
            'installer/setup/setup_util_unittest.cc',
            'installer/setup/setup_util_unittest.h',
            'installer/setup/update_active_setup_version_work_item_unittest.cc',
            'installer/setup/user_hive_visitor_unittest.cc',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        },
      ],
    }],
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'launcher_support64',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '..',
            ],
          },
          'defines': [
              '<@(nacl_win64_defines)',
          ],
              'dependencies': [
              '<(DEPTH)/base/base.gyp:base_win64',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
          'sources': [
            'installer/launcher_support/chrome_launcher_support.cc',
            'installer/launcher_support/chrome_launcher_support.h',
          ],
        },
      ],
    }],
    ['OS=="linux" and branding=="Chrome"', {
      'variables': {
        # Always google_chrome since this only applies to branding==Chrome.
        'branding_dir': 'app/theme/google_chrome',
        'branding_dir_100': 'app/theme/default_100_percent/google_chrome',
        'version' : '<!(python <(version_py_path) -f ../chrome/VERSION -t "@MAJOR@.@MINOR@.@BUILD@.@PATCH@")',
        'revision' : '<!(python ../build/util/lastchange.py --revision-only)',
        'packaging_files_common': [
          'installer/linux/common/apt.include',
          'installer/linux/common/default-app.template',
          'installer/linux/common/default-app-block.template',
          'installer/linux/common/desktop.template',
          'installer/linux/common/google-chrome/google-chrome.info',
          'installer/linux/common/installer.include',
          'installer/linux/common/postinst.include',
          'installer/linux/common/prerm.include',
          'installer/linux/common/repo.cron',
          'installer/linux/common/rpm.include',
          'installer/linux/common/rpmrepo.cron',
          'installer/linux/common/symlinks.include',
          'installer/linux/common/variables.include',
          'installer/linux/common/wrapper',
        ],
        'packaging_files_deb': [
          'installer/linux/debian/build.sh',
          'installer/linux/debian/changelog.template',
          'installer/linux/debian/control.template',
          'installer/linux/debian/debian.menu',
          'installer/linux/debian/expected_deps_ia32',
          'installer/linux/debian/expected_deps_x64',
          'installer/linux/debian/postinst',
          'installer/linux/debian/postrm',
          'installer/linux/debian/prerm',
        ],
        'packaging_files_rpm': [
          'installer/linux/rpm/build.sh',
          'installer/linux/rpm/chrome.spec.template',
          'installer/linux/rpm/expected_deps_i386',
          'installer/linux/rpm/expected_deps_x86_64',
        ],
        'packaging_files_binaries': [
          # TODO(mmoss) Any convenient way to get all the relevant build
          # files? (e.g. all locales, resources, etc.)
          '<(PRODUCT_DIR)/chrome',
          '<(PRODUCT_DIR)/chrome_sandbox',
          '<(PRODUCT_DIR)/xdg-mime',
          '<(PRODUCT_DIR)/xdg-settings',
          '<(PRODUCT_DIR)/locales/en-US.pak',
          '<(PRODUCT_DIR)/nacl_helper',
          '<(PRODUCT_DIR)/nacl_helper_bootstrap',
          '<(PRODUCT_DIR)/PepperFlash/libpepflashplayer.so',
          '<(PRODUCT_DIR)/PepperFlash/manifest.json',
          '<@(default_apps_list_linux_dest)',
        ],
        'flock_bash': ['flock', '--', '/tmp/linux_package_lock', 'bash'],
        'deb_build': '<(PRODUCT_DIR)/installer/debian/build.sh',
        'rpm_build': '<(PRODUCT_DIR)/installer/rpm/build.sh',
        # The script expects either "google_chrome" or "chromium" for -d,
        # which is also what branding_path_component contains.
        'deb_cmd': ['<@(flock_bash)', '<(deb_build)', '-o' '<(PRODUCT_DIR)',
                    '-b', '<(PRODUCT_DIR)', '-a', '<(target_arch)',
                    '-d', '<(branding_path_component)'],
        'rpm_cmd': ['<@(flock_bash)', '<(rpm_build)', '-o' '<(PRODUCT_DIR)',
                    '-b', '<(PRODUCT_DIR)', '-a', '<(target_arch)',
                    '-d', '<(branding_path_component)'],
        'conditions': [
          ['target_arch=="ia32"', {
            'deb_arch': 'i386',
            'rpm_arch': 'i386',
            'packaging_files_binaries': [
              '<(PRODUCT_DIR)/nacl_irt_x86_32.nexe',
              '<(PRODUCT_DIR)/<(widevine_cdm_path)/libwidevinecdmadapter.so',
              '<(PRODUCT_DIR)/<(widevine_cdm_path)/libwidevinecdm.so',
            ],
            'packaging_files_common': [
              '<(DEPTH)/build/linux/bin/eu-strip',
            ],
          }],
          ['target_arch=="x64"', {
            'deb_arch': 'amd64',
            'rpm_arch': 'x86_64',
            'packaging_files_binaries': [
              '<(PRODUCT_DIR)/nacl_irt_x86_64.nexe',
              '<(PRODUCT_DIR)/<(widevine_cdm_path)/libwidevinecdmadapter.so',
              '<(PRODUCT_DIR)/<(widevine_cdm_path)/libwidevinecdm.so',
            ],
            'packaging_files_common': [
              '<!(which eu-strip)',
            ],
          }],
          ['target_arch=="arm"', {
            'deb_arch': 'arm',
            'rpm_arch': 'arm',
          }],
          ['asan==1', {
            'packaging_files_binaries': [
              '<(PRODUCT_DIR)/lib/libc++.so',
            ],
          }],
        ],
      },
      'targets': [
        {
          'target_name': 'linux_installer_configs',
          'type': 'none',
          # Add these files to the build output so the build archives will be
          # "hermetic" for packaging. This is only for branding="Chrome" since
          # we only create packages for official builds.
          'copies': [
            {
              # GN version: //chrome/installer/linux:deb_packaging_files
              'destination': '<(PRODUCT_DIR)/installer/debian/',
              'files': [
                '<@(packaging_files_deb)',
              ]
            },
            {
              # GN version: //chrome/installer/linux:rpm_packaging_files
              'destination': '<(PRODUCT_DIR)/installer/rpm/',
              'files': [
                '<@(packaging_files_rpm)',
              ]
            },
            {
              # GN version: //chrome/installer/linux:common_packaging_files
              'destination': '<(PRODUCT_DIR)/installer/common/',
              'files': [
                '<@(packaging_files_common)',
              ]
            },
            # Additional theme resources needed for package building.
            {
              # GN version: //chrome/installer/linux:theme_files
              'destination': '<(PRODUCT_DIR)/installer/theme/',
              'files': [
                '<(branding_dir)/linux/product_logo_32.xpm',
                '<(branding_dir_100)/product_logo_16.png',
                '<(branding_dir)/product_logo_22.png',
                '<(branding_dir)/product_logo_24.png',
                '<(branding_dir_100)/product_logo_32.png',
                '<(branding_dir)/product_logo_48.png',
                '<(branding_dir)/product_logo_64.png',
                '<(branding_dir)/product_logo_128.png',
                '<(branding_dir)/product_logo_256.png',
                '<(branding_dir)/BRANDING',
              ],
            },
          ],
          'actions': [
            {
              # GN version: //chrome/installer/linux:save_build_info
              'action_name': 'save_build_info',
              'inputs': [
                '<(branding_dir)/BRANDING',
                '<(version_path)',
                '<(lastchange_path)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/installer/version.txt',
              ],
              # Just output the default version info variables.
              'action': [
                'python', '<(version_py_path)',
                '-f', '<(branding_dir)/BRANDING',
                '-f', '<(version_path)',
                '-f', '<(lastchange_path)',
                '-o', '<@(_outputs)'
              ],
            },
          ],
        },
        {
          # GN version: //chrome/installer/linux
          'target_name': 'linux_packages_all',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'linux_packages_unstable',
            'linux_packages_beta',
            'linux_packages_stable',
          ],
        },
        {
          # 'asan' is a developer, testing-only package, so it shouldn't be
          # included in the 'linux_packages_all' collection.
          'target_name': 'linux_packages_asan',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'linux_packages_asan_deb',
          ],
          # ChromeOS doesn't care about RPM packages.
          'conditions': [
            ['chromeos==0', {
              'dependencies': [
                'linux_packages_asan_rpm',
              ],
            }],
          ],
        },
        {
          # 'trunk' is a developer, testing-only package, so it shouldn't be
          # included in the 'linux_packages_all' collection.
          'target_name': 'linux_packages_trunk',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'linux_packages_trunk_deb',
          ],
          # ChromeOS doesn't care about RPM packages.
          'conditions': [
            ['chromeos==0', {
              'dependencies': [
                'linux_packages_trunk_rpm',
              ],
            }],
          ],
        },
        {
          # GN version: //chrome/installer/linux:unstable
          'target_name': 'linux_packages_unstable',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'linux_packages_unstable_deb',
          ],
          # ChromeOS doesn't care about RPM packages.
          'conditions': [
            ['chromeos==0', {
              'dependencies': [
                'linux_packages_unstable_rpm',
              ],
            }],
          ],
        },
        {
          # GN version: //chrome/installer/linux:beta
          'target_name': 'linux_packages_beta',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'linux_packages_beta_deb',
          ],
          # ChromeOS doesn't care about RPM packages.
          'conditions': [
            ['chromeos==0', {
              'dependencies': [
                'linux_packages_beta_rpm',
              ],
            }],
          ],
        },
        {
          # GN version: //chrome/installer/linux:stable
          'target_name': 'linux_packages_stable',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'linux_packages_stable_deb',
          ],
          # ChromeOS doesn't care about RPM packages.
          'conditions': [
            ['chromeos==0', {
              'dependencies': [
                'linux_packages_stable_rpm',
              ],
            }],
          ],
        },
        # TODO(mmoss) gyp looping construct would be handy here ...
        # These package actions are the same except for the 'channel' variable.
        {
          # GN version: //chrome/installer/linux:asan
          'target_name': 'linux_packages_asan_deb',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'chrome',
            'linux_installer_configs',
          ],
          'actions': [
            {
              'variables': {
                'channel': 'asan',
              },
              'action_name': 'deb_packages_<(channel)',
              'process_outputs_as_sources': 1,
              'inputs': [
                '<(deb_build)',
                '<@(packaging_files_binaries)',
                '<@(packaging_files_common)',
                '<@(packaging_files_deb)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/google-chrome-<(channel)_<(version)-1_<(deb_arch).deb',
              ],
              'action': [ '<@(deb_cmd)', '-c', '<(channel)', ],
            },
          ],
        },
        {
          # GN version: //chrome/installer/linux:trunk
          'target_name': 'linux_packages_trunk_deb',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'chrome',
            'linux_installer_configs',
          ],
          'actions': [
            {
              'variables': {
                'channel': 'trunk',
              },
              'action_name': 'deb_packages_<(channel)',
              'process_outputs_as_sources': 1,
              'inputs': [
                '<(deb_build)',
                '<@(packaging_files_binaries)',
                '<@(packaging_files_common)',
                '<@(packaging_files_deb)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/google-chrome-<(channel)_<(version)-1_<(deb_arch).deb',
              ],
              'action': [ '<@(deb_cmd)', '-c', '<(channel)', ],
            },
          ],
        },
        {
          # GN version: //chrome/installer/linux:unstable
          'target_name': 'linux_packages_unstable_deb',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'chrome',
            'linux_installer_configs',
          ],
          'actions': [
            {
              'variables': {
                'channel': 'unstable',
              },
              'action_name': 'deb_packages_<(channel)',
              'process_outputs_as_sources': 1,
              'inputs': [
                '<(deb_build)',
                '<@(packaging_files_binaries)',
                '<@(packaging_files_common)',
                '<@(packaging_files_deb)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/google-chrome-<(channel)_<(version)-1_<(deb_arch).deb',
              ],
              'action': [ '<@(deb_cmd)', '-c', '<(channel)', ],
            },
          ],
        },
        {
          # GN version: //chrome/installer/linux:beta
          'target_name': 'linux_packages_beta_deb',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'chrome',
            'linux_installer_configs',
          ],
          'actions': [
            {
              'variables': {
                'channel': 'beta',
              },
              'action_name': 'deb_packages_<(channel)',
              'process_outputs_as_sources': 1,
              'inputs': [
                '<(deb_build)',
                '<@(packaging_files_binaries)',
                '<@(packaging_files_common)',
                '<@(packaging_files_deb)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/google-chrome-<(channel)_<(version)-1_<(deb_arch).deb',
              ],
              'action': [ '<@(deb_cmd)', '-c', '<(channel)', ],
            },
          ],
        },
        {
          # GN version: //chrome/installer/linux:stable
          'target_name': 'linux_packages_stable_deb',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'chrome',
            'linux_installer_configs',
          ],
          'actions': [
            {
              'variables': {
                'channel': 'stable',
              },
              'action_name': 'deb_packages_<(channel)',
              'process_outputs_as_sources': 1,
              'inputs': [
                '<(deb_build)',
                '<@(packaging_files_binaries)',
                '<@(packaging_files_common)',
                '<@(packaging_files_deb)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/google-chrome-<(channel)_<(version)-1_<(deb_arch).deb',
              ],
              'action': [ '<@(deb_cmd)', '-c', '<(channel)', ],
            },
          ],
        },
        {
          # GN version: //chrome/installer/linux:asan
          'target_name': 'linux_packages_asan_rpm',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'chrome',
            'linux_installer_configs',
          ],
          'actions': [
            {
              'variables': {
                'channel': 'asan',
              },
              'action_name': 'rpm_packages_<(channel)',
              'process_outputs_as_sources': 1,
              'inputs': [
                '<(rpm_build)',
                '<(PRODUCT_DIR)/installer/rpm/chrome.spec.template',
                '<@(packaging_files_binaries)',
                '<@(packaging_files_common)',
                '<@(packaging_files_rpm)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/google-chrome-<(channel)-<(version)-1.<(rpm_arch).rpm',
              ],
              'action': [ '<@(rpm_cmd)', '-c', '<(channel)', ],
            },
          ],
        },
        {
          # GN version: //chrome/installer/linux:trunk
          'target_name': 'linux_packages_trunk_rpm',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'chrome',
            'linux_installer_configs',
          ],
          'actions': [
            {
              'variables': {
                'channel': 'trunk',
              },
              'action_name': 'rpm_packages_<(channel)',
              'process_outputs_as_sources': 1,
              'inputs': [
                '<(rpm_build)',
                '<(PRODUCT_DIR)/installer/rpm/chrome.spec.template',
                '<@(packaging_files_binaries)',
                '<@(packaging_files_common)',
                '<@(packaging_files_rpm)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/google-chrome-<(channel)-<(version)-1.<(rpm_arch).rpm',
              ],
              'action': [ '<@(rpm_cmd)', '-c', '<(channel)', ],
            },
          ],
        },
        {
          # GN version: //chrome/installer/linux:unstable
          'target_name': 'linux_packages_unstable_rpm',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'chrome',
            'linux_installer_configs',
          ],
          'actions': [
            {
              'variables': {
                'channel': 'unstable',
              },
              'action_name': 'rpm_packages_<(channel)',
              'process_outputs_as_sources': 1,
              'inputs': [
                '<(rpm_build)',
                '<(PRODUCT_DIR)/installer/rpm/chrome.spec.template',
                '<@(packaging_files_binaries)',
                '<@(packaging_files_common)',
                '<@(packaging_files_rpm)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/google-chrome-<(channel)-<(version)-1.<(rpm_arch).rpm',
              ],
              'action': [ '<@(rpm_cmd)', '-c', '<(channel)', ],
            },
          ],
        },
        {
          # GN version: //chrome/installer/linux:beta
          'target_name': 'linux_packages_beta_rpm',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'chrome',
            'linux_installer_configs',
          ],
          'actions': [
            {
              'variables': {
                'channel': 'beta',
              },
              'action_name': 'rpm_packages_<(channel)',
              'process_outputs_as_sources': 1,
              'inputs': [
                '<(rpm_build)',
                '<(PRODUCT_DIR)/installer/rpm/chrome.spec.template',
                '<@(packaging_files_binaries)',
                '<@(packaging_files_common)',
                '<@(packaging_files_rpm)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/google-chrome-<(channel)-<(version)-1.<(rpm_arch).rpm',
              ],
              'action': [ '<@(rpm_cmd)', '-c', '<(channel)', ],
            },
          ],
        },
        {
          # GN version: //chrome/installer/linux:stable
          'target_name': 'linux_packages_stable_rpm',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'chrome',
            'linux_installer_configs',
          ],
          'actions': [
            {
              'variables': {
                'channel': 'stable',
              },
              'action_name': 'rpm_packages_<(channel)',
              'process_outputs_as_sources': 1,
              'inputs': [
                '<(rpm_build)',
                '<(PRODUCT_DIR)/installer/rpm/chrome.spec.template',
                '<@(packaging_files_binaries)',
                '<@(packaging_files_common)',
                '<@(packaging_files_rpm)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/google-chrome-<(channel)-<(version)-1.<(rpm_arch).rpm',
              ],
              'action': [ '<@(rpm_cmd)', '-c', '<(channel)', ],
            },
          ],
        },
      ],
    }],
    ['OS=="mac"', {
      'variables': {
        'mac_packaging_dir':
            '<(PRODUCT_DIR)/<(mac_product_name) Packaging',
        # <(PRODUCT_DIR) expands to $(BUILT_PRODUCTS_DIR), which doesn't
        # work properly in a shell script, where ${BUILT_PRODUCTS_DIR} is
        # needed.
        'mac_packaging_sh_dir':
            '${BUILT_PRODUCTS_DIR}/<(mac_product_name) Packaging',
      }, # variables
      'targets': [
        {
          'target_name': 'installer_packaging',
          'type': 'none',
          'dependencies': [
            'installer/mac/third_party/bsdiff/goobsdiff.gyp:*',
            'installer/mac/third_party/xz/xz.gyp:*',
          ],
          'conditions': [
            ['buildtype=="Official"', {
              'actions': [
                {
                  # Create sign.sh, the script that the packaging system will
                  # use to sign the .app bundle.
                  'action_name': 'Make sign.sh',
                  'variables': {
                    'make_signers_sh_path': 'installer/mac/make_signers.sh',
                  },
                  'inputs': [
                    '<(make_signers_sh_path)',
                    'installer/mac/sign_app.sh.in',
                    'installer/mac/sign_versioned_dir.sh.in',
                    'installer/mac/app_resource_rules.plist.in',
                    '<(version_path)',
                  ],
                  'outputs': [
                    '<(mac_packaging_dir)/sign_app.sh',
                    '<(mac_packaging_dir)/sign_versioned_dir.sh',
                    '<(mac_packaging_dir)/app_resource_rules.plist',
                  ],
                  'action': [
                    '<(make_signers_sh_path)',
                    '<(mac_packaging_sh_dir)',
                    '<(mac_product_name)',
                    '<(version_full)',
                  ],
                },
              ],  # actions
            }],  # buildtype=="Official"
          ],  # conditions
          'copies': [
            {
              # Put the files where the packaging system will find them. The
              # packager will use these when building the "full installer"
              # disk images and delta/differential update disk images.
              'destination': '<(mac_packaging_dir)',
              'files': [
                '<(PRODUCT_DIR)/goobsdiff',
                '<(PRODUCT_DIR)/goobspatch',
                '<(PRODUCT_DIR)/liblzma_decompress.dylib',
                '<(PRODUCT_DIR)/xz',
                '<(PRODUCT_DIR)/xzdec',
                'installer/mac/dirdiffer.sh',
                'installer/mac/dirpatcher.sh',
                'installer/mac/dmgdiffer.sh',
                'installer/mac/pkg-dmg',
                'installer/mac/sign_installer_tools.sh',
              ],
              'conditions': [
                ['mac_keystone==1', {
                  'files': [
                    'installer/mac/keystone_install.sh',
                  ],
                }],  # mac_keystone
                ['branding=="Chrome" and buildtype=="Official"', {
                  'files': [
                    'app/theme/google_chrome/mac/app_canary.icns',
                    'app/theme/google_chrome/mac/document_canary.icns',
                    'installer/mac/internal/chrome_canary_dmg_dsstore',
                    'installer/mac/internal/chrome_canary_dmg_icon.icns',
                    'installer/mac/internal/chrome_dmg_background.png',
                    'installer/mac/internal/chrome_dmg_dsstore',
                    'installer/mac/internal/chrome_dmg_icon.icns',
                    'installer/mac/internal/generate_dmgs',
                  ],
                }],  # branding=="Chrome" and buildtype=="Official"
              ],  # conditions
            },
          ],  # copies
        },  # target: installer_packaging
        {
          'target_name': 'gcapi_lib',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'sources': [
            'installer/gcapi_mac/gcapi.h',
            'installer/gcapi_mac/gcapi.mm',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
            ],
          },
          'xcode_settings': {
            'ARCHS': [ 'i386', 'x86_64' ],
            'MACOSX_DEPLOYMENT_TARGET': '10.5',
            'GCC_ENABLE_OBJC_GC': 'supported',
          },
        },
        {
          'target_name': 'gcapi_example',
          'type': 'executable',
          'dependencies': [
            'gcapi_lib',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'installer/gcapi_mac/gcapi_example_client.mm',
          ],
        },
      ],  # targets
    }],  # OS=="mac"
    ['OS=="win" and test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'installer_util_unittests_run',
          'type': 'none',
          'dependencies': [
            'installer_util_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'installer_util_unittests.isolate',
          ],
        },
        {
          'target_name': 'setup_unittests_run',
          'type': 'none',
          'dependencies': [
            'setup_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'setup_unittests.isolate',
          ],
        },
      ],
    }],
  ],
}
