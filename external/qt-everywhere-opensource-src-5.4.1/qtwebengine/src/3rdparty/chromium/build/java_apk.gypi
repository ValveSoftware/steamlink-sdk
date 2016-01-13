# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule
# to build Android APKs in a consistent manner.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'my_package_apk',
#   'type': 'none',
#   'variables': {
#     'apk_name': 'MyPackage',
#     'java_in_dir': 'path/to/package/root',
#     'resource_dir': 'path/to/package/root/res',
#   },
#   'includes': ['path/to/this/gypi/file'],
# }
#
# Required variables:
#  apk_name - The final apk will be named <apk_name>.apk
#  java_in_dir - The top-level java directory. The src should be in
#    <(java_in_dir)/src.
# Optional/automatic variables:
#  additional_input_paths - These paths will be included in the 'inputs' list to
#    ensure that this target is rebuilt when one of these paths changes.
#  additional_res_dirs - Additional directories containing Android resources.
#  additional_res_packages - Package names of the R.java files corresponding to
#    each directory in additional_res_dirs.
#  additional_src_dirs - Additional directories with .java files to be compiled
#    and included in the output of this target.
#  asset_location - The directory where assets are located.
#  generated_src_dirs - Same as additional_src_dirs except used for .java files
#    that are generated at build time. This should be set automatically by a
#    target's dependencies. The .java files in these directories are not
#    included in the 'inputs' list (unlike additional_src_dirs).
#  input_jars_paths - The path to jars to be included in the classpath. This
#    should be filled automatically by depending on the appropriate targets.
#  is_test_apk - Set to 1 if building a test apk.  This prevents resources from
#    dependencies from being re-included.
#  native_lib_target - The target_name of the target which generates the final
#    shared library to be included in this apk. A stripped copy of the
#    library will be included in the apk.
#  resource_dir - The directory for resources.
#  R_package - A custom Java package to generate the resource file R.java in.
#    By default, the package given in AndroidManifest.xml will be used.
#  use_chromium_linker - Enable the content dynamic linker that allows sharing the
#    RELRO section of the native libraries between the different processes.
#  enable_chromium_linker_tests - Enable the content dynamic linker test support
#    code. This allows a test APK to inject a Linker.TestRunner instance at
#    runtime. Should only be used by the chromium_linker_test_apk target!!
#  never_lint - Set to 1 to not run lint on this target.
{
  'variables': {
    'tested_apk_obfuscated_jar_path%': '/',
    'tested_apk_dex_path%': '/',
    'additional_input_paths': [],
    'input_jars_paths': [],
    'library_dexed_jars_paths': [],
    'additional_src_dirs': [],
    'generated_src_dirs': [],
    'app_manifest_version_name%': '<(android_app_version_name)',
    'app_manifest_version_code%': '<(android_app_version_code)',
    # aapt generates this proguard.txt.
    'generated_proguard_file': '<(intermediate_dir)/proguard.txt',
    'proguard_enabled%': 'false',
    'proguard_flags_paths': ['<(generated_proguard_file)'],
    'jar_name': 'chromium_apk_<(_target_name).jar',
    'resource_dir%':'<(DEPTH)/build/android/ant/empty/res',
    'R_package%':'',
    'additional_R_text_files': [],
    'dependencies_res_zip_paths': [],
    'additional_res_packages': [],
    'is_test_apk%': 0,
    'resource_input_paths': [],
    'intermediate_dir': '<(PRODUCT_DIR)/<(_target_name)',
    'asset_location%': '<(intermediate_dir)/assets',
    'codegen_stamp': '<(intermediate_dir)/codegen.stamp',
    'package_input_paths': [],
    'ordered_libraries_file': '<(intermediate_dir)/native_libraries.json',
    'native_libraries_template': '<(DEPTH)/base/android/java/templates/NativeLibraries.template',
    'native_libraries_java_dir': '<(intermediate_dir)/native_libraries_java/',
    'native_libraries_java_file': '<(native_libraries_java_dir)/NativeLibraries.java',
    'native_libraries_java_stamp': '<(intermediate_dir)/native_libraries_java.stamp',
    'native_libraries_template_data_dir': '<(intermediate_dir)/native_libraries/',
    'native_libraries_template_data_file': '<(native_libraries_template_data_dir)/native_libraries_array.h',
    'native_libraries_template_version_file': '<(native_libraries_template_data_dir)/native_libraries_version.h',
    'compile_stamp': '<(intermediate_dir)/compile.stamp',
    'lint_stamp': '<(intermediate_dir)/lint.stamp',
    'lint_result': '<(intermediate_dir)/lint_result.xml',
    'lint_config': '<(intermediate_dir)/lint_config.xml',
    'never_lint%': 0,
    'instr_stamp': '<(intermediate_dir)/instr.stamp',
    'jar_stamp': '<(intermediate_dir)/jar.stamp',
    'obfuscate_stamp': '<(intermediate_dir)/obfuscate.stamp',
    'strip_stamp': '<(intermediate_dir)/strip.stamp',
    'classes_dir': '<(intermediate_dir)/classes',
    'classes_final_dir': '<(intermediate_dir)/classes_instr',
    'javac_includes': [],
    'jar_excluded_classes': [],
    'jar_path': '<(PRODUCT_DIR)/lib.java/<(jar_name)',
    'obfuscated_jar_path': '<(intermediate_dir)/obfuscated.jar',
    'test_jar_path': '<(PRODUCT_DIR)/test.lib.java/<(apk_name).jar',
    'dex_path': '<(intermediate_dir)/classes.dex',
    'emma_device_jar': '<(android_sdk_root)/tools/lib/emma_device.jar',
    'android_manifest_path%': '<(java_in_dir)/AndroidManifest.xml',
    'push_stamp': '<(intermediate_dir)/push.stamp',
    'link_stamp': '<(intermediate_dir)/link.stamp',
    'package_resources_stamp': '<(intermediate_dir)/package_resources.stamp',
    'resource_zip_path': '<(intermediate_dir)/<(_target_name).resources.zip',
    'resource_packaged_apk_name': '<(apk_name)-resources.ap_',
    'resource_packaged_apk_path': '<(intermediate_dir)/<(resource_packaged_apk_name)',
    'unsigned_apk_path': '<(intermediate_dir)/<(apk_name)-unsigned.apk',
    'final_apk_path%': '<(PRODUCT_DIR)/apks/<(apk_name).apk',
    'incomplete_apk_path': '<(intermediate_dir)/<(apk_name)-incomplete.apk',
    'apk_install_record': '<(intermediate_dir)/apk_install.record.stamp',
    'device_intermediate_dir': '/data/data/org.chromium.gyp_managed_install/<(_target_name)/<(CONFIGURATION_NAME)',
    'symlink_script_host_path': '<(intermediate_dir)/create_symlinks.sh',
    'symlink_script_device_path': '<(device_intermediate_dir)/create_symlinks.sh',
    'create_standalone_apk%': 1,
    'res_v14_verify_only%': 0,
    'variables': {
      'variables': {
        'native_lib_target%': '',
        'native_lib_version_name%': '',
        'use_chromium_linker%' : 0,
        'enable_chromium_linker_tests%': 0,
        'is_test_apk%': 0,
      },
      'conditions': [
        ['gyp_managed_install == 1 and native_lib_target != ""', {
          'unsigned_standalone_apk_path': '<(intermediate_dir)/<(apk_name)-standalone-unsigned.apk',
        }, {
          'unsigned_standalone_apk_path': '<(unsigned_apk_path)',
        }],
        ['gyp_managed_install == 1', {
          'apk_package_native_libs_dir': '<(intermediate_dir)/libs.managed',
        }, {
          'apk_package_native_libs_dir': '<(intermediate_dir)/libs',
        }],
        ['is_test_apk == 0 and emma_coverage != 0', {
          'emma_instrument%': 1,
        },{
          'emma_instrument%': 0,
        }],
      ],
    },
    'native_lib_target%': '',
    'native_lib_version_name%': '',
    'use_chromium_linker%' : 0,
    'enable_chromium_linker_tests%': 0,
    'emma_instrument%': '<(emma_instrument)',
    'apk_package_native_libs_dir': '<(apk_package_native_libs_dir)',
    'unsigned_standalone_apk_path': '<(unsigned_standalone_apk_path)',
    'extra_native_libs': [],
    'apk_dex_input_paths': [ '>@(library_dexed_jars_paths)' ],
  },
  # Pass the jar path to the apk's "fake" jar target.  This would be better as
  # direct_dependent_settings, but a variable set by a direct_dependent_settings
  # cannot be lifted in a dependent to all_dependent_settings.
  'all_dependent_settings': {
    'conditions': [
      ['proguard_enabled == "true"', {
        'variables': {
          'proguard_enabled': 'true',
        }
      }],
    ],
    'variables': {
      'apk_output_jar_path': '<(jar_path)',
      'tested_apk_obfuscated_jar_path': '<(obfuscated_jar_path)',
      'tested_apk_dex_path': '<(dex_path)',
    },
  },
  'conditions': [
    ['resource_dir!=""', {
      'variables': {
        'resource_input_paths': [ '<!@(find <(resource_dir) -name "*")' ]
      },
    }],
    ['R_package != ""', {
      'variables': {
        # We generate R.java in package R_package (in addition to the package
        # listed in the AndroidManifest.xml, which is unavoidable).
        'additional_res_packages': ['<(R_package)'],
        'additional_R_text_files': ['<(PRODUCT_DIR)/<(package_name)/R.txt'],
      },
    }],
    ['native_lib_target != "" and component == "shared_library"', {
      'dependencies': [
        '<(DEPTH)/build/android/setup.gyp:copy_system_libraries',
      ],
    }],
    ['use_chromium_linker == 1', {
      'dependencies': [
        '<(DEPTH)/base/base.gyp:chromium_android_linker',
      ],
    }],
    ['native_lib_target != ""', {
      'variables': {
        'generated_src_dirs': [ '<(native_libraries_java_dir)' ],
        'native_libs_paths': [
          '<(SHARED_LIB_DIR)/<(native_lib_target).>(android_product_extension)'
        ],
        'package_input_paths': [
          '<(apk_package_native_libs_dir)/<(android_app_abi)/gdbserver',
        ],
      },
      'copies': [
        {
          # gdbserver is always copied into the APK's native libs dir. The ant
          # build scripts (apkbuilder task) will only include it in a debug
          # build.
          'destination': '<(apk_package_native_libs_dir)/<(android_app_abi)',
          'files': [
            '<(android_gdbserver)',
          ],
        },
      ],
      'actions': [
        {
          'variables': {
            'conditions': [
              ['use_chromium_linker == 1', {
                'variables': {
                  'linker_input_libraries': [
                    '<(SHARED_LIB_DIR)/libchromium_android_linker.>(android_product_extension)',
                  ],
                }
              }, {
                'variables': {
                  'linker_input_libraries': [],
                },
              }],
            ],
            'input_libraries': [
              '<@(native_libs_paths)',
              '<@(extra_native_libs)',
              '<@(linker_input_libraries)',
            ],
          },
          'includes': ['../build/android/write_ordered_libraries.gypi'],
        },
        {
          'action_name': 'native_libraries_template_data_<(_target_name)',
          'message': 'Creating native_libraries_list.h for <(_target_name)',
          'inputs': [
            '<(DEPTH)/build/android/gyp/util/build_utils.py',
            '<(DEPTH)/build/android/gyp/create_native_libraries_header.py',
            '<(ordered_libraries_file)',
          ],
          'outputs': [
            '<(native_libraries_template_data_file)',
            '<(native_libraries_template_version_file)',
          ],
          'action': [
            'python', '<(DEPTH)/build/android/gyp/create_native_libraries_header.py',
            '--ordered-libraries=<(ordered_libraries_file)',
            '--version-name=<(native_lib_version_name)',
            '--native-library-list=<(native_libraries_template_data_file)',
            '--version-output=<(native_libraries_template_version_file)',
          ],
        },
        {
          'action_name': 'native_libraries_<(_target_name)',
          'variables': {
            'conditions': [
              ['use_chromium_linker == 1', {
                'variables': {
                  'linker_gcc_preprocess_defines': [
                    '--defines', 'ENABLE_CHROMIUM_LINKER',
                  ],
                }
              }, {
                'variables': {
                  'linker_gcc_preprocess_defines': [],
                },
              }],
              ['enable_chromium_linker_tests == 1', {
                'variables': {
                  'linker_tests_gcc_preprocess_defines': [
                    '--defines', 'ENABLE_CHROMIUM_LINKER_TESTS',
                  ],
                }
              }, {
                'variables': {
                  'linker_tests_gcc_preprocess_defines': [],
                },
              }],
            ],
            'gcc_preprocess_defines': [
              '<@(linker_gcc_preprocess_defines)',
              '<@(linker_tests_gcc_preprocess_defines)',
            ],
          },
          'message': 'Creating NativeLibraries.java for <(_target_name)',
          'inputs': [
            '<(DEPTH)/build/android/gyp/util/build_utils.py',
            '<(DEPTH)/build/android/gyp/gcc_preprocess.py',
            '<(native_libraries_template_data_file)',
            '<(native_libraries_template_version_file)',
            '<(native_libraries_template)',
          ],
          'outputs': [
            '<(native_libraries_java_stamp)',
          ],
          'action': [
            'python', '<(DEPTH)/build/android/gyp/gcc_preprocess.py',
            '--include-path=<(native_libraries_template_data_dir)',
            '--output=<(native_libraries_java_file)',
            '--template=<(native_libraries_template)',
            '--stamp=<(native_libraries_java_stamp)',
            '<@(gcc_preprocess_defines)',
          ],
        },
        {
          'action_name': 'strip_native_libraries',
          'variables': {
            'ordered_libraries_file%': '<(ordered_libraries_file)',
            'stripped_libraries_dir': '<(libraries_source_dir)',
            'input_paths': [
              '<@(native_libs_paths)',
              '<@(extra_native_libs)',
            ],
            'stamp': '<(strip_stamp)'
          },
          'includes': ['../build/android/strip_native_libraries.gypi'],
        },
      ],
      'conditions': [
        ['gyp_managed_install == 1', {
          'variables': {
            'libraries_top_dir': '<(intermediate_dir)/lib.stripped',
            'libraries_source_dir': '<(libraries_top_dir)/lib/<(android_app_abi)',
            'device_library_dir': '<(device_intermediate_dir)/lib.stripped',
            'configuration_name': '<(CONFIGURATION_NAME)',
          },
          'dependencies': [
            '<(DEPTH)/build/android/setup.gyp:get_build_device_configurations',
          ],
          'actions': [
            {
              'includes': ['../build/android/push_libraries.gypi'],
            },
            {
              'action_name': 'create device library symlinks',
              'message': 'Creating links on device for <(_target_name)',
              'inputs': [
                '<(DEPTH)/build/android/gyp/util/build_utils.py',
                '<(DEPTH)/build/android/gyp/create_device_library_links.py',
                '<(apk_install_record)',
                '<(build_device_config_path)',
                '<(ordered_libraries_file)',
              ],
              'outputs': [
                '<(link_stamp)'
              ],
              'action': [
                'python', '<(DEPTH)/build/android/gyp/create_device_library_links.py',
                '--build-device-configuration=<(build_device_config_path)',
                '--libraries-json=<(ordered_libraries_file)',
                '--script-host-path=<(symlink_script_host_path)',
                '--script-device-path=<(symlink_script_device_path)',
                '--target-dir=<(device_library_dir)',
                '--apk=<(incomplete_apk_path)',
                '--stamp=<(link_stamp)',
                '--configuration-name=<(CONFIGURATION_NAME)',
              ],
            },
          ],
          'conditions': [
            ['create_standalone_apk == 1', {
              'actions': [
                {
                  'action_name': 'create standalone APK',
                  'variables': {
                    'inputs': [
                      '<(ordered_libraries_file)',
                      '<(strip_stamp)',
                    ],
                    'input_apk_path': '<(unsigned_apk_path)',
                    'output_apk_path': '<(unsigned_standalone_apk_path)',
                    'libraries_top_dir%': '<(libraries_top_dir)',
                  },
                  'includes': [ 'android/create_standalone_apk_action.gypi' ],
                },
              ],
            }],
          ],
        }, {
          # gyp_managed_install != 1
          'variables': {
            'libraries_source_dir': '<(apk_package_native_libs_dir)/<(android_app_abi)',
            'package_input_paths': [ '<(strip_stamp)' ],
          },
        }],
      ],
    }], # native_lib_target != ''
    ['gyp_managed_install == 0 or create_standalone_apk == 1', {
      'actions': [
        {
          'action_name': 'finalize standalone apk',
          'variables': {
            'input_apk_path': '<(unsigned_standalone_apk_path)',
            'output_apk_path': '<(final_apk_path)',
          },
          'includes': [ 'android/finalize_apk_action.gypi']
        },
      ],
    }],
    ['gyp_managed_install == 1', {
      'actions': [
        {
          'action_name': 'finalize incomplete apk',
          'variables': {
            'input_apk_path': '<(unsigned_apk_path)',
            'output_apk_path': '<(incomplete_apk_path)',
          },
          'includes': [ 'android/finalize_apk_action.gypi']
        },
        {
          'action_name': 'apk_install_<(_target_name)',
          'message': 'Installing <(apk_name).apk',
          'inputs': [
            '<(DEPTH)/build/android/gyp/util/build_utils.py',
            '<(DEPTH)/build/android/gyp/apk_install.py',
            '<(build_device_config_path)',
            '<(incomplete_apk_path)',
          ],
          'outputs': [
            '<(apk_install_record)',
          ],
          'action': [
            'python', '<(DEPTH)/build/android/gyp/apk_install.py',
            '--apk-path=<(incomplete_apk_path)',
            '--build-device-configuration=<(build_device_config_path)',
            '--install-record=<(apk_install_record)',
            '--configuration-name=<(CONFIGURATION_NAME)',
          ],
        },
      ],
    }],
    ['is_test_apk == 1', {
      'dependencies': [
        '<(DEPTH)/tools/android/android_tools.gyp:android_tools',
      ]
    }],
  ],
  'dependencies': [
    '<(DEPTH)/tools/android/md5sum/md5sum.gyp:md5sum',
  ],
  'actions': [
    {
      'action_name': 'process_resources',
      'message': 'processing resources for <(_target_name)',
      'variables': {
        # Write the inputs list to a file, so that its mtime is updated when
        # the list of inputs changes.
        'inputs_list_file': '>|(apk_codegen.<(_target_name).gypcmd >@(additional_input_paths) >@(resource_input_paths))',
        'process_resources_options': [],
        'conditions': [
          ['is_test_apk == 1', {
            'dependencies_res_zip_paths=': [],
            'additional_res_packages=': [],
          }],
          ['res_v14_verify_only == 1', {
            'process_resources_options': ['--v14-verify-only']
          }],
        ],
      },
      'inputs': [
        '<(DEPTH)/build/android/gyp/util/build_utils.py',
        '<(DEPTH)/build/android/gyp/process_resources.py',
        '<(android_manifest_path)',
        '>@(additional_input_paths)',
        '>@(resource_input_paths)',
        '>@(dependencies_res_zip_paths)',
        '>(inputs_list_file)',
      ],
      'outputs': [
        '<(resource_zip_path)',
        '<(generated_proguard_file)',
        '<(codegen_stamp)',
      ],
      'action': [
        'python', '<(DEPTH)/build/android/gyp/process_resources.py',
        '--android-sdk', '<(android_sdk)',
        '--android-sdk-tools', '<(android_sdk_tools)',

        '--android-manifest', '<(android_manifest_path)',
        '--dependencies-res-zips', '>(dependencies_res_zip_paths)',

        '--extra-res-packages', '>(additional_res_packages)',
        '--extra-r-text-files', '>(additional_R_text_files)',

        '--proguard-file', '<(generated_proguard_file)',

        '--resource-dirs', '<(resource_dir)',
        '--resource-zip-out', '<(resource_zip_path)',

        '--R-dir', '<(intermediate_dir)/gen',

        '--stamp', '<(codegen_stamp)',

        '<@(process_resources_options)',
      ],
    },
    {
      'action_name': 'javac_<(_target_name)',
      'message': 'Compiling java for <(_target_name)',
      'variables': {
        'gen_src_dirs': [
          '<(intermediate_dir)/gen',
          '>@(generated_src_dirs)',
        ],
        # If there is a separate find for additional_src_dirs, it will find the
        # wrong .java files when additional_src_dirs is empty.
        # TODO(thakis): Gyp caches >! evaluation by command. Both java.gypi and
        # java_apk.gypi evaluate the same command, and at the moment two targets
        # set java_in_dir to "java". Add a dummy comment here to make sure
        # that the two targets (one uses java.gypi, the other java_apk.gypi)
        # get distinct source lists. Medium-term, make targets list all their
        # Java files instead of using find. (As is, this will be broken if two
        # targets use the same java_in_dir and both use java_apk.gypi or
        # both use java.gypi.)
        'java_sources': ['>!@(find >(java_in_dir)/src >(additional_src_dirs) -name "*.java"  # apk)'],

      },
      'inputs': [
        '<(DEPTH)/build/android/gyp/util/build_utils.py',
        '<(DEPTH)/build/android/gyp/javac.py',
        '>@(java_sources)',
        '>@(input_jars_paths)',
        '<(codegen_stamp)',
      ],
      'conditions': [
        ['native_lib_target != ""', {
          'inputs': [ '<(native_libraries_java_stamp)' ],
        }],
      ],
      'outputs': [
        '<(compile_stamp)',
      ],
      'action': [
        'python', '<(DEPTH)/build/android/gyp/javac.py',
        '--output-dir=<(classes_dir)',
        '--classpath=>(input_jars_paths) <(android_sdk_jar)',
        '--src-gendirs=>(gen_src_dirs)',
        '--javac-includes=<(javac_includes)',
        '--chromium-code=<(chromium_code)',
        '--stamp=<(compile_stamp)',
        '>@(java_sources)',
      ],
    },
    {
      'variables': {
        'src_dirs': [
          '<(java_in_dir)/src',
          '>@(additional_src_dirs)',
        ],
        'stamp_path': '<(lint_stamp)',
        'result_path': '<(lint_result)',
        'config_path': '<(lint_config)',
      },
      'inputs': [
        '<(compile_stamp)',
      ],
      'outputs': [
        '<(lint_stamp)',
      ],
      'includes': [ 'android/lint_action.gypi' ],
    },
    {
      'action_name': 'instr_classes_<(_target_name)',
      'message': 'Instrumenting <(_target_name) classes',
      'variables': {
        'input_path': '<(classes_dir)',
        'output_path': '<(classes_final_dir)',
        'stamp_path': '<(instr_stamp)',
        'instr_type': 'classes',
      },
      'inputs': [
        '<(compile_stamp)',
      ],
      'outputs': [
        '<(instr_stamp)',
      ],
      'includes': [ 'android/instr_action.gypi' ],
    },
    {
      'action_name': 'jar_<(_target_name)',
      'message': 'Creating <(_target_name) jar',
      'inputs': [
        '<(DEPTH)/build/android/gyp/util/build_utils.py',
        '<(DEPTH)/build/android/gyp/util/md5_check.py',
        '<(DEPTH)/build/android/gyp/jar.py',
        '<(instr_stamp)',
      ],
      'outputs': [
        '<(jar_stamp)',
        '<(jar_path)',
      ],
      'action': [
        'python', '<(DEPTH)/build/android/gyp/jar.py',
        '--classes-dir=<(classes_final_dir)',
        '--jar-path=<(jar_path)',
        '--excluded-classes=<(jar_excluded_classes)',
        '--stamp=<(jar_stamp)',
      ]
    },
    {
      'action_name': 'obfuscate_<(_target_name)',
      'message': 'Obfuscating <(_target_name)',
      'variables': {
        'additional_obfuscate_options': [],
        'additional_obfuscate_input_paths': [],
        'proguard_out_dir': '<(intermediate_dir)/proguard',
        'proguard_input_jar_paths': [
          '>@(input_jars_paths)',
          '<(jar_path)',
        ],
        'target_conditions': [
          ['is_test_apk == 1', {
            'additional_obfuscate_options': [
              '--testapp',
            ],
          }],
          ['is_test_apk == 1 and tested_apk_obfuscated_jar_path != "/"', {
            'additional_obfuscate_options': [
              '--tested-apk-obfuscated-jar-path', '>(tested_apk_obfuscated_jar_path)',
            ],
            'additional_obfuscate_input_paths': [
              '>(tested_apk_obfuscated_jar_path).info',
            ],
          }],
          ['proguard_enabled == "true"', {
            'additional_obfuscate_options': [
              '--proguard-enabled',
            ],
          }],
        ],
        'obfuscate_input_jars_paths': [
          '>@(input_jars_paths)',
          '<(jar_path)',
        ],
      },
      'conditions': [
        ['is_test_apk == 1', {
          'outputs': [
            '<(test_jar_path)',
          ],
        }],
      ],
      'inputs': [
        '<(DEPTH)/build/android/gyp/apk_obfuscate.py',
        '<(DEPTH)/build/android/gyp/util/build_utils.py',
        '>@(proguard_flags_paths)',
        '>@(obfuscate_input_jars_paths)',
        '>@(additional_obfuscate_input_paths)',
        '<(instr_stamp)',
      ],
      'outputs': [
        '<(obfuscate_stamp)',

        # In non-Release builds, these paths will all be empty files.
        '<(obfuscated_jar_path)',
        '<(obfuscated_jar_path).info',
        '<(obfuscated_jar_path).dump',
        '<(obfuscated_jar_path).seeds',
        '<(obfuscated_jar_path).mapping',
        '<(obfuscated_jar_path).usage',
      ],
      'action': [
        'python', '<(DEPTH)/build/android/gyp/apk_obfuscate.py',

        '--configuration-name', '<(CONFIGURATION_NAME)',

        '--android-sdk', '<(android_sdk)',
        '--android-sdk-tools', '<(android_sdk_tools)',
        '--android-sdk-jar', '<(android_sdk_jar)',

        '--input-jars-paths=>(proguard_input_jar_paths)',
        '--proguard-configs=>(proguard_flags_paths)',


        '--test-jar-path', '<(test_jar_path)',
        '--obfuscated-jar-path', '<(obfuscated_jar_path)',

        '--proguard-jar-path', '<(android_sdk_root)/tools/proguard/lib/proguard.jar',

        '--stamp', '<(obfuscate_stamp)',

        '>@(additional_obfuscate_options)',
      ],
    },
    {
      'action_name': 'dex_<(_target_name)',
      'variables': {
        'output_path': '<(dex_path)',
        'dex_input_paths': [
          '>@(apk_dex_input_paths)',
          '<(jar_path)',
        ],
        'proguard_enabled_input_path': '<(obfuscated_jar_path)',
      },
      'target_conditions': [
        ['emma_instrument != 0', {
          'dex_no_locals': 1,
          'dex_input_paths': [
            '<(emma_device_jar)'
          ],
        }],
        ['is_test_apk == 1 and tested_apk_dex_path != "/"', {
          'variables': {
            'dex_additional_options': [
              '--excluded-paths-file', '>(tested_apk_dex_path).inputs'
            ],
          },
          'inputs': [
            '>(tested_apk_dex_path).inputs',
          ],
        }],
        ['proguard_enabled == "true"', {
          'inputs': [ '<(obfuscate_stamp)' ]
        }, {
          'inputs': [ '<(instr_stamp)' ]
        }],
      ],
      'includes': [ 'android/dex_action.gypi' ],
    },
    {
      'action_name': 'package_resources',
      'message': 'packaging resources for <(_target_name)',
      'variables': {
        'package_resource_zip_input_paths': [
          '<(resource_zip_path)',
          '>@(dependencies_res_zip_paths)',
        ],
      },
      'conditions': [
        ['is_test_apk == 1', {
          'variables': {
            'dependencies_res_zip_paths=': [],
            'additional_res_packages=': [],
          }
        }],
      ],
      'inputs': [
        # TODO: This isn't always rerun correctly, http://crbug.com/351928
        '<(DEPTH)/build/android/gyp/util/build_utils.py',
        '<(DEPTH)/build/android/gyp/package_resources.py',
        '<(android_manifest_path)',

        '>@(package_resource_zip_input_paths)',

        '<(codegen_stamp)',
      ],
      'outputs': [
        '<(resource_packaged_apk_path)',
      ],
      'action': [
        'python', '<(DEPTH)/build/android/gyp/package_resources.py',
        '--android-sdk', '<(android_sdk)',
        '--android-sdk-tools', '<(android_sdk_tools)',

        '--configuration-name', '<(CONFIGURATION_NAME)',

        '--android-manifest', '<(android_manifest_path)',
        '--version-code', '<(app_manifest_version_code)',
        '--version-name', '<(app_manifest_version_name)',

        '--asset-dir', '<(asset_location)',
        '--resource-zips', '>(package_resource_zip_input_paths)',

        '--apk-path', '<(resource_packaged_apk_path)',
      ],
    },
    {
      'action_name': 'ant_package_<(_target_name)',
      'message': 'Packaging <(_target_name)',
      'variables': {
        # Write the inputs list to a file, so that its mtime is updated when
        # the list of inputs changes.
        'inputs_list_file': '>|(apk_package.<(_target_name).gypcmd >@(package_input_paths))'
      },
      'inputs': [
        '<(DEPTH)/build/android/ant/apk-package.xml',
        '<(DEPTH)/build/android/gyp/util/build_utils.py',
        '<(DEPTH)/build/android/gyp/ant.py',
        '<(dex_path)',
        '<(codegen_stamp)',
        '<(obfuscate_stamp)',
        '<(resource_packaged_apk_path)',
        '>@(package_input_paths)',
        '>(inputs_list_file)',
      ],
      'outputs': [
        '<(unsigned_apk_path)',
      ],
      'action': [
        'python', '<(DEPTH)/build/android/gyp/ant.py',
        '-quiet',
        '-DANDROID_SDK_ROOT=<(android_sdk_root)',
        '-DANDROID_SDK_TOOLS=<(android_sdk_tools)',
        '-DRESOURCE_PACKAGED_APK_NAME=<(resource_packaged_apk_name)',
        '-DAPK_NAME=<(apk_name)',
        '-DCONFIGURATION_NAME=<(CONFIGURATION_NAME)',
        '-DNATIVE_LIBS_DIR=<(apk_package_native_libs_dir)',
        '-DOUT_DIR=<(intermediate_dir)',
        '-DUNSIGNED_APK_PATH=<(unsigned_apk_path)',
        '-DEMMA_INSTRUMENT=<(emma_instrument)',
        '-DEMMA_DEVICE_JAR=<(emma_device_jar)',

        '-Dbasedir=.',
        '-buildfile',
        '<(DEPTH)/build/android/ant/apk-package.xml',
      ]
    },
  ],
}
