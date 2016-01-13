# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'lastchange_path': '../build/util/LASTCHANGE',
    'libpeer_target_type%': 'static_library',
    # 'branding_dir' is set in the 'conditions' section at the bottom.
  },
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'gcapi_dll',
          'type': 'loadable_module',
          'dependencies': [
            'gcapi_lib',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'installer/gcapi/gcapi.def',
            'installer/gcapi/gcapi_dll.cc',
          ],
        },
        {
          'target_name': 'gcapi_lib',
          'type': 'static_library',
          'dependencies': [
            'installer_util',
            '../base/base.gyp:base',
            '../chrome/chrome.gyp:launcher_support',
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
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        },
        {
          'target_name': 'gcapi_test',
          'type': 'executable',
          'dependencies': [
            'common',
            'gcapi_dll',
            'gcapi_lib',
            'installer_util',
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
            '../testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'installer/gcapi/gcapi_last_run_test.cc',
            'installer/gcapi/gcapi_omaha_experiment_test.cc',
            'installer/gcapi/gcapi_reactivation_test.cc',
            'installer/gcapi/gcapi_test_registry_overrider.cc',
            'installer/gcapi/gcapi_test_registry_overrider.h',
            'installer/gcapi/gcapi_test.cc',
            'installer/gcapi/gcapi_test.rc',
            'installer/gcapi/resource.h',
          ],
        },
        {
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
            '../content/content.gyp:content_common',
            '../testing/gmock.gyp:gmock',
            '../testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'installer/setup/compat_checks_unittest.cc',
            'installer/setup/setup_constants.cc',
            'installer/util/advanced_firewall_manager_win_unittest.cc',
            'installer/util/callback_work_item_unittest.cc',
            'installer/util/channel_info_unittest.cc',
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
            'installer/util/installer_util_unittests.rc',
            'installer/util/installer_util_unittests_resource.h',
            'installer/util/language_selector_unittest.cc',
            'installer/util/legacy_firewall_manager_win_unittest.cc',
            'installer/util/logging_installer_unittest.cc',
            'installer/util/lzma_util_unittest.cc',
            'installer/util/master_preferences_unittest.cc',
            'installer/util/move_tree_work_item_unittest.cc',
            'installer/util/product_state_unittest.cc',
            'installer/util/product_unittest.cc',
            'installer/util/product_unittest.h',
            'installer/util/registry_key_backup_unittest.cc',
            'installer/util/registry_test_data.cc',
            'installer/util/registry_test_data.h',
            'installer/util/run_all_unittests.cc',
            'installer/util/self_cleaning_temp_dir_unittest.cc',
            'installer/util/set_reg_value_work_item_unittest.cc',
            'installer/util/shell_util_unittest.cc',
            'installer/util/uninstall_metrics_unittest.cc',
            'installer/util/wmi_unittest.cc',
            'installer/util/work_item_list_unittest.cc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
          ],
          'msvs_settings': {
            'VCManifestTool': {
              'AdditionalManifestFiles': [
                '$(ProjectDir)\\installer\\mini_installer\\mini_installer.exe.manifest',
              ],
            },
          },
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        },
        {
          'target_name': 'installer_util_strings',
          'type': 'none',
          'actions': [
            {
              'action_name': 'installer_util_strings',
              'variables': {
                'create_string_rc_py': 'installer/util/prebuild/create_string_rc.py',
              },
              'conditions': [
                ['branding=="Chrome"', {
                  'variables': {
                    'brand_strings': 'google_chrome_strings',
                  },
                }, {
                  'variables': {
                    'brand_strings': 'chromium_strings',
                  },
                }],
              ],
              'inputs': [
                '<(create_string_rc_py)',
                'app/<(brand_strings).grd',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/installer_util_strings/installer_util_strings.h',
                '<(SHARED_INTERMEDIATE_DIR)/installer_util_strings/installer_util_strings.rc',
              ],
              'action': ['python',
                         '<(create_string_rc_py)',
                         '-i', 'app/<(brand_strings).grd:resources',
                         '-n', 'installer_util_strings',
                         '-o', '<(SHARED_INTERMEDIATE_DIR)/installer_util_strings',],
              'message': 'Generating installer_util_strings',
            },
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)/installer_util_strings',
            ],
          },
        },
        {
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
          'target_name': 'setup',
          'type': 'executable',
          'dependencies': [
            'installer_util',
            'installer_util_strings',
            'launcher_support',
            '../base/base.gyp:base',
            '../breakpad/breakpad.gyp:breakpad_handler',
            '../chrome/common_constants.gyp:common_constants',
            '../rlz/rlz.gyp:rlz_lib',
            '../third_party/zlib/zlib.gyp:zlib',
          ],
          'include_dirs': [
            '..',
            '<(INTERMEDIATE_DIR)',
            '<(SHARED_INTERMEDIATE_DIR)/setup',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)/setup',
            ],
          },
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/installer_util_strings/installer_util_strings.rc',
            'installer/mini_installer/chrome.release',
            'installer/setup/archive_patch_helper.cc',
            'installer/setup/archive_patch_helper.h',
            'installer/setup/install.cc',
            'installer/setup/install.h',
            'installer/setup/install_worker.cc',
            'installer/setup/install_worker.h',
            'installer/setup/setup_main.cc',
            'installer/setup/setup_main.h',
            'installer/setup/setup.ico',
            'installer/setup/setup.rc',
            'installer/setup/setup_constants.cc',
            'installer/setup/setup_constants.h',
            'installer/setup/setup_exe_version.rc.version',
            'installer/setup/setup_resource.h',
            'installer/setup/setup_util.cc',
            'installer/setup/setup_util.h',
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
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
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
            # TODO(mark):  <(branding_dir) should be defined by the
            # global condition block at the bottom of the file, but
            # this doesn't work due to the following issue:
            #
            #   http://code.google.com/p/gyp/issues/detail?id=22
            #
            # Remove this block once the above issue is fixed.
            [ 'branding == "Chrome"', {
              'variables': {
                 'branding_dir': 'app/theme/google_chrome',
                 'branding_dir_100': 'app/theme/default_100_percent/google_chrome',
              },
            }, { # else branding!="Chrome"
              'variables': {
                 'branding_dir': 'app/theme/chromium',
                 'branding_dir_100': 'app/theme/default_100_percent/chromium',
              },
            }],
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
          'target_name': 'setup_unittests',
          'type': 'executable',
          'dependencies': [
            'installer_util',
            'installer_util_strings',
            'launcher_support',
            '../base/base.gyp:base',
            '../base/base.gyp:base_i18n',
            '../base/base.gyp:test_support_base',
            '../testing/gmock.gyp:gmock',
            '../testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '..',
            '<(INTERMEDIATE_DIR)',
          ],
          # TODO(robertshield): Move the items marked with "Move to lib"
          # below into a separate lib and then link both setup.exe and
          # setup_unittests.exe against that.
          'sources': [
            'installer/mini_installer/chrome.release',  # Move to lib
            'installer/mini_installer/appid.h',
            'installer/mini_installer/chrome_appid.cc',
            'installer/mini_installer/configuration.cc',
            'installer/mini_installer/configuration.h',
            'installer/mini_installer/configuration_test.cc',
            'installer/mini_installer/decompress.cc',
            'installer/mini_installer/decompress.h',
            'installer/mini_installer/decompress_test.cc',
            'installer/mini_installer/mini_string.cc',
            'installer/mini_installer/mini_string.h',
            'installer/mini_installer/mini_string_test.cc',
            'installer/setup/archive_patch_helper.cc',  # Move to lib
            'installer/setup/archive_patch_helper.h',   # Move to lib
            'installer/setup/archive_patch_helper_unittest.cc',
            'installer/setup/install.cc',               # Move to lib
            'installer/setup/install.h',                # Move to lib
            'installer/setup/install_unittest.cc',
            'installer/setup/install_worker.cc',        # Move to lib
            'installer/setup/install_worker.h',         # Move to lib
            'installer/setup/install_worker_unittest.cc',
            'installer/setup/run_all_unittests.cc',
            'installer/setup/setup_constants.cc',       # Move to lib
            'installer/setup/setup_constants.h',        # Move to lib
            'installer/setup/setup_unittests.rc',
            'installer/setup/setup_unittests_resource.h',
            'installer/setup/setup_util.cc',
            'installer/setup/setup_util_unittest.cc',
            'installer/setup/setup_util_unittest.h',
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
          'installer/linux/debian/expected_deps',
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
          '<(PRODUCT_DIR)/libffmpegsumo.so',
          '<(PRODUCT_DIR)/libpdf.so',
          '<(PRODUCT_DIR)/libppGoogleNaClPluginChrome.so',
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
        'deb_cmd': ['<@(flock_bash)', '<(deb_build)', '-o' '<(PRODUCT_DIR)',
                    '-b', '<(PRODUCT_DIR)', '-a', '<(target_arch)'],
        'rpm_cmd': ['<@(flock_bash)', '<(rpm_build)', '-o' '<(PRODUCT_DIR)',
                    '-b', '<(PRODUCT_DIR)', '-a', '<(target_arch)'],
        'conditions': [
          ['target_arch=="ia32"', {
            'deb_arch': 'i386',
            'rpm_arch': 'i386',
            'packaging_files_binaries': [
              '<(PRODUCT_DIR)/nacl_irt_x86_32.nexe',
              '<(PRODUCT_DIR)/libwidevinecdmadapter.so',
              '<(PRODUCT_DIR)/libwidevinecdm.so',
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
              '<(PRODUCT_DIR)/libwidevinecdmadapter.so',
              '<(PRODUCT_DIR)/libwidevinecdm.so',
            ],
            'packaging_files_common': [
              '<!(which eu-strip)',
            ],
          }],
          ['target_arch=="arm"', {
            'deb_arch': 'arm',
            'rpm_arch': 'arm',
          }],
          ['libpeer_target_type!="static_library"', {
            'packaging_files_binaries': [
              '<(PRODUCT_DIR)/lib/libpeerconnection.so',
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
              'destination': '<(PRODUCT_DIR)/installer/debian/',
              'files': [
                '<@(packaging_files_deb)',
              ]
            },
            {
              'destination': '<(PRODUCT_DIR)/installer/rpm/',
              'files': [
                '<@(packaging_files_rpm)',
              ]
            },
            {
              'destination': '<(PRODUCT_DIR)/installer/common/',
              'files': [
                '<@(packaging_files_common)',
              ]
            },
            # Additional theme resources needed for package building.
            {
              'destination': '<(PRODUCT_DIR)/installer/theme/',
              'files': [
                '<(branding_dir_100)/product_logo_16.png',
                '<(branding_dir)/product_logo_22.png',
                '<(branding_dir)/product_logo_24.png',
                '<(branding_dir_100)/product_logo_32.png',
                '<(branding_dir)/product_logo_48.png',
                '<(branding_dir)/product_logo_64.png',
                '<(branding_dir)/product_logo_128.png',
                '<(branding_dir)/product_logo_256.png',
                '<(branding_dir)/product_logo_32.xpm',
                '<(branding_dir)/BRANDING',
              ],
            },
          ],
          'actions': [
            {
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
            'MACOSX_DEPLOYMENT_TARGET': '10.4',
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
    [ 'branding == "Chrome"', {
      'variables': {
         'branding_dir': 'app/theme/google_chrome',
         'branding_dir_100': 'app/theme/default_100_percent/google_chrome',
      },
    }, { # else branding!="Chrome"
      'variables': {
         'branding_dir': 'app/theme/chromium',
         'branding_dir_100': 'app/theme/default_100_percent/chromium',
      },
    }],
  ],
}
