# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    # Included to get 'mac_bundle_id' and other variables.
    '../build/chrome_settings.gypi',
  ],
  'variables': {
    'chromium_code': 1,
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/chrome',
    'policy_out_dir': '<(SHARED_INTERMEDIATE_DIR)/policy',
    'protoc_out_dir': '<(SHARED_INTERMEDIATE_DIR)/protoc_out',
    'generate_policy_source_script_path':
        'policy/tools/generate_policy_source.py',
    'policy_constant_header_path':
        '<(policy_out_dir)/policy/policy_constants.h',
    'policy_constant_source_path':
        '<(policy_out_dir)/policy/policy_constants.cc',
    'protobuf_decoder_path':
        '<(policy_out_dir)/policy/cloud_policy_generated.cc',
    'app_restrictions_path':
        '<(policy_out_dir)/app_restrictions.xml',
    'risk_tag_header_path':
        '<(policy_out_dir)/risk_tag.h',
    # This is the "full" protobuf, which defines one protobuf message per
    # policy. It is also the format currently used by the server.
    'chrome_settings_proto_path':
        '<(policy_out_dir)/policy/chrome_settings.proto',
    # This protobuf is equivalent to chrome_settings.proto but shares messages
    # for policies of the same type, so that less classes have to be generated
    # and compiled.
    'cloud_policy_proto_path':
        '<(policy_out_dir)/policy/cloud_policy.proto',
  },
  'conditions': [
    ['component=="static_library"', {
      'targets': [
        {
          # GN version: //components/policy:policy_component
          'target_name': 'policy_component',
          'type': 'none',
          'dependencies': [
            'policy_component_common',
            'policy_component_browser',
          ],
        },
        {
          # GN version: //components/policy:policy_component_common
          'target_name': 'policy_component_common',
          'type': 'static_library',
          'includes': [
            'policy/policy_common.gypi',
          ],
        },
        {
          # GN version: //components/policy:policy_component_browser
          'target_name': 'policy_component_browser',
          'type': 'static_library',
          'dependencies': [
            'policy_component_common',
          ],
          'includes': [
            'policy/policy_browser.gypi',
          ],
          'conditions': [
            ['OS=="android"', {
              'dependencies': ['policy_jni_headers']},
            ],
          ],
        },
      ],
    }, {  # component=="shared_library"
      'targets': [
        {
          # GN version: //components/policy:policy_component
          'target_name': 'policy_component',
          'type': 'shared_library',
          'includes': [
            'policy/policy_common.gypi',
            'policy/policy_browser.gypi',
          ],
        },
        {
          # GN version: //components/policy:policy_component_common
          'target_name': 'policy_component_common',
          'type': 'none',
          'dependencies': [
            'policy_component',
          ],
          'conditions': [
            ['OS=="android"', {
              'dependencies': ['policy_jni_headers']},
            ],
          ],
        },
        {
          # GN version: //components/policy:policy_component_browser
          'target_name': 'policy_component_browser',
          'type': 'none',
          'dependencies': [
            'policy_component',
          ],
        },
      ],
    }],
    ['configuration_policy==1', {
      'targets': [
        {
          # GN version: //components/policy:cloud_policy_code_generate
          'target_name': 'cloud_policy_code_generate',
          'type': 'none',
          'actions': [
            {
              'inputs': [
                'policy/resources/policy_templates.json',
                '<(DEPTH)/chrome/VERSION',
                '<(generate_policy_source_script_path)',
              ],
              'outputs': [
                '<(policy_constant_header_path)',
                '<(policy_constant_source_path)',
                '<(protobuf_decoder_path)',
                '<(chrome_settings_proto_path)',
                '<(cloud_policy_proto_path)',
                '<(app_restrictions_path)',
                '<(risk_tag_header_path)',
              ],
              'action_name': 'generate_policy_source',
              'action': [
                'python',
                '<@(generate_policy_source_script_path)',
                '--policy-constants-header=<(policy_constant_header_path)',
                '--policy-constants-source=<(policy_constant_source_path)',
                '--chrome-settings-protobuf=<(chrome_settings_proto_path)',
                '--cloud-policy-protobuf=<(cloud_policy_proto_path)',
                '--cloud-policy-decoder=<(protobuf_decoder_path)',
                '--app-restrictions-definition=<(app_restrictions_path)',
                '--risk-tag-header=<(risk_tag_header_path)',
                '<(DEPTH)/chrome/VERSION',
                '<(OS)',
                '<(chromeos)',
                'policy/resources/policy_templates.json',
              ],
              'message': 'Generating policy source',
              'conditions': [
                ['OS!="android"', {
                  'outputs!': [
                    '<(app_restrictions_path)',
                  ],
                }],
              ],
            },
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(policy_out_dir)',
              '<(protoc_out_dir)',
            ],
          },
        },
        {
          # GN version: //components/policy:cloud_policy_proto_generated_compile
          'target_name': 'cloud_policy_proto_generated_compile',
          'type': '<(component)',
          'sources': [
            '<(cloud_policy_proto_path)',
          ],
          'variables': {
            'proto_in_dir': '<(policy_out_dir)/policy',
            'proto_out_dir': 'policy/proto',
            'cc_generator_options': 'dllexport_decl=POLICY_PROTO_EXPORT:',
            'cc_include': 'components/policy/policy_proto_export.h',
          },
          'dependencies': [
            'cloud_policy_code_generate',
          ],
          'includes': [
            '../build/protoc.gypi',
          ],
          'defines': [
            'POLICY_PROTO_COMPILATION',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        },
        {
          # This target builds the "full" protobuf, used for tests only.
          # GN version: //components/policy:chrome_settings_proto_generated_compile
          'target_name': 'chrome_settings_proto_generated_compile',
          'type': 'static_library',
          'sources': [
            '<(chrome_settings_proto_path)',
          ],
          'variables': {
            'proto_in_dir': '<(policy_out_dir)/policy',
            'proto_out_dir': 'policy/proto',
          },
          'dependencies': [
            'cloud_policy_code_generate',
            'cloud_policy_proto_generated_compile',
          ],
          'includes': [
            '../build/protoc.gypi',
          ],
        },
        {
          # GN version: //components/policy
          'target_name': 'policy',
          'type': 'static_library',
          'hard_dependency': 1,
          'direct_dependent_settings': {
            'include_dirs': [
              '<(policy_out_dir)',
              '<(protoc_out_dir)',
            ],
          },
          'sources': [
            '<(policy_constant_header_path)',
            '<(policy_constant_source_path)',
            '<(risk_tag_header_path)',
            '<(protobuf_decoder_path)',
          ],
          'include_dirs': [
            '<(DEPTH)',
          ],
          'dependencies': [
            'cloud_policy_code_generate',
            'cloud_policy_proto_generated_compile',
            '<(DEPTH)/base/base.gyp:base',
            '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
          ],
          'defines': [
            'POLICY_COMPONENT_IMPLEMENTATION',
          ],
        },
        {
          # GN version: //components/policy/proto
          'target_name': 'cloud_policy_proto',
          'type': '<(component)',
          'sources': [
            'policy/proto/chrome_extension_policy.proto',
            'policy/proto/device_management_backend.proto',
            'policy/proto/device_management_local.proto',
            'policy/proto/policy_signing_key.proto',
          ],
          'variables': {
            'proto_in_dir': 'policy/proto',
            'proto_out_dir': 'policy/proto',
            'cc_generator_options': 'dllexport_decl=POLICY_PROTO_EXPORT:',
            'cc_include': 'components/policy/policy_proto_export.h',
          },
          'includes': [
            '../build/protoc.gypi',
          ],
          'conditions': [
            ['OS=="android" or OS=="ios"', {
              'sources!': [
                'policy/proto/chrome_extension_policy.proto',
              ],
            }],
            ['chromeos==0', {
              'sources!': [
                'policy/proto/device_management_local.proto',
              ],
            }],
          ],
          'defines': [
            'POLICY_PROTO_COMPILATION',
          ],
        },
        {
          # GN version: //components/policy:test_support
          'target_name': 'policy_test_support',
          'type': 'none',
          'hard_dependency': 1,
          'direct_dependent_settings': {
            'include_dirs': [
              '<(policy_out_dir)',
              '<(protoc_out_dir)',
            ],
          },
          'dependencies': [
            'chrome_settings_proto_generated_compile',
            'policy',
          ],
        },
        {
          # GN version: //components/policy:policy_component_test_support
          'target_name': 'policy_component_test_support',
          'type': 'static_library',
          # This must be undefined so that POLICY_EXPORT works correctly in
          # the static_library build.
          'defines!': [
            'POLICY_COMPONENT_IMPLEMENTATION',
          ],
          'dependencies': [
            'cloud_policy_proto',
            'policy_component',
            'policy_test_support',
            '../testing/gmock.gyp:gmock',
            '../testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'policy/core/browser/configuration_policy_pref_store_test.cc',
            'policy/core/browser/configuration_policy_pref_store_test.h',
            'policy/core/common/cloud/mock_cloud_external_data_manager.cc',
            'policy/core/common/cloud/mock_cloud_external_data_manager.h',
            'policy/core/common/cloud/mock_cloud_policy_client.cc',
            'policy/core/common/cloud/mock_cloud_policy_client.h',
            'policy/core/common/cloud/mock_cloud_policy_store.cc',
            'policy/core/common/cloud/mock_cloud_policy_store.h',
            'policy/core/common/cloud/mock_device_management_service.cc',
            'policy/core/common/cloud/mock_device_management_service.h',
            'policy/core/common/cloud/mock_user_cloud_policy_store.cc',
            'policy/core/common/cloud/mock_user_cloud_policy_store.h',
            'policy/core/common/cloud/policy_builder.cc',
            'policy/core/common/cloud/policy_builder.h',
            'policy/core/common/configuration_policy_provider_test.cc',
            'policy/core/common/configuration_policy_provider_test.h',
            'policy/core/common/fake_async_policy_loader.cc',
            'policy/core/common/fake_async_policy_loader.h',
            'policy/core/common/mock_configuration_policy_provider.cc',
            'policy/core/common/mock_configuration_policy_provider.h',
            'policy/core/common/mock_policy_service.cc',
            'policy/core/common/mock_policy_service.h',
            'policy/core/common/policy_test_utils.cc',
            'policy/core/common/policy_test_utils.h',
            'policy/core/common/preferences_mock_mac.cc',
            'policy/core/common/preferences_mock_mac.h',
            'policy/core/common/remote_commands/test_remote_command_job.cc',
            'policy/core/common/remote_commands/test_remote_command_job.h',
            'policy/core/common/remote_commands/testing_remote_commands_server.cc',
            'policy/core/common/remote_commands/testing_remote_commands_server.h',
          ],
          'conditions': [
            ['OS=="android"', {
              'sources!': [
                'policy/core/common/fake_async_policy_loader.cc',
                'policy/core/common/fake_async_policy_loader.h',
              ],
            }],
            ['chromeos==1', {
              'sources!': [
                'policy/core/common/cloud/mock_user_cloud_policy_store.cc',
                'policy/core/common/cloud/mock_user_cloud_policy_store.h',
              ],
            }],
          ],
        },
      ],
    }],
    ['OS=="android"',
     {
      'targets' : [
        {
          'target_name' : 'policy_jni_headers',
          'type': 'none',
          'sources': [
            'policy/android/java/src/org/chromium/policy/CombinedPolicyProvider.java',
            'policy/android/java/src/org/chromium/policy/PolicyConverter.java',
           ],
          'variables': {
            'jni_gen_package': 'policy',
           },
          'includes': [ '../build/jni_generator.gypi' ],
         },
         {
           'target_name': 'components_policy_junit_tests',
           'type': 'none',
           'dependencies': [
             '../testing/android/junit/junit_test.gyp:junit_test_support',
           ],
           'variables': {
             'main_class': 'org.chromium.testing.local.JunitTestMain',
             'src_paths': [
               '../testing/android/junit/DummyTest.java',
             ],
             'wrapper_script_name': 'helper/<(_target_name)',
           },
           'includes': [ '../build/host_jar.gypi' ],
         },
       ],
    }],
    ['OS=="android" and configuration_policy==1', {
      'targets': [
        {
          'target_name': 'app_restrictions_resources',
          'type': 'none',
          'variables': {
            'resources_zip': '<(PRODUCT_DIR)/res.java/<(_target_name).zip',
            'input_resources_dir':
            '<(SHARED_INTERMEDIATE_DIR)/chrome/app/policy/android',
            'create_zip_script': '../build/android/gyp/zip.py',
          },
          'copies': [
            {
              'destination': '<(input_resources_dir)/xml-v21/',
              'files': [
                '<(SHARED_INTERMEDIATE_DIR)/policy/app_restrictions.xml'
              ],
            },
          ],
          'actions': [
            {
              'action_name': 'create_resources_zip',
              'inputs': [
                '<(create_zip_script)',
                '<(input_resources_dir)/xml-v21/app_restrictions.xml',
                '<(input_resources_dir)/values-v21/restriction_values.xml',
              ],
              'outputs': [
                '<(resources_zip)'
              ],
              'action': [
                'python', '<(create_zip_script)',
                '--input-dir', '<(input_resources_dir)',
                '--output', '<(resources_zip)',
              ],
            }
          ],
          'all_dependent_settings': {
            'variables': {
              'additional_input_paths': ['<(resources_zip)'],
              'dependencies_res_zip_paths': ['<(resources_zip)'],
            },
          },
        },
        {
          # GN: //components/policy/android:policy_java
          'target_name': 'policy_java',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_java',
          ],
          'variables': {
            'java_in_dir': 'policy/android/java',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          # GN: //components/policy/android:policy_java_test_support
          'target_name': 'policy_java_test_support',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_java',
            '../base/base.gyp:base_java_test_support',
            'policy_java'
          ],
          'variables': {
            'java_in_dir': 'policy/android/javatests',
          },
          'includes': [ '../build/java.gypi' ],
        },
      ],
    }],
    ['OS=="win" and target_arch=="ia32" and configuration_policy==1', {
      'targets': [
        {
          'target_name': 'policy_win64',
          'type': 'static_library',
          'hard_dependency': 1,
          'sources': [
            '<(policy_constant_header_path)',
            '<(policy_constant_source_path)',
            '<(risk_tag_header_path)',
          ],
          'include_dirs': [
            '<(DEPTH)',
          ],
          'direct_dependent_settings':  {
            'include_dirs': [
              '<(policy_out_dir)'
            ],
          },
          'dependencies': [
            'cloud_policy_code_generate',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
    ['OS!="ios"', {
      'targets': [
        {
          # policy_templates has different inputs and outputs, so it can't use
          # the rules of chrome_strings
          'target_name': 'policy_templates',
          'type': 'none',
          'variables': {
            'grit_grd_file': 'policy/resources/policy_templates.grd',
            'grit_info_cmd': [
              'python',
              '<(DEPTH)/tools/grit/grit_info.py',
              '<@(grit_defines)',
            ],
          },
          'includes': [
            '../build/grit_target.gypi',
          ],
          'actions': [
            {
              'action_name': 'policy_templates',
              'includes': [
                '../build/grit_action.gypi',
              ],
            },
          ],
        },
      ],
    }],
    ['OS=="mac"', {
      'targets': [
        {
          # This is the bundle of the manifest file of Chrome.
          # It contains the manifest file and its string tables.
          'target_name': 'chrome_manifest_bundle',
          'type': 'loadable_module',
          'mac_bundle': 1,
          'product_extension': 'manifest',
          'product_name': '<(mac_bundle_id)',
          'variables': {
            # This avoids stripping debugging symbols from the target, which
            # would fail because there is no binary code here.
            'mac_strip': 0,
          },
          'dependencies': [
             # Provides app-Manifest.plist and its string tables:
            'policy_templates',
          ],
          'actions': [
            {
              'action_name': 'Copy MCX manifest file to manifest bundle',
              'inputs': [
                '<(grit_out_dir)/app/policy/mac/app-Manifest.plist',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/app_manifest/<(mac_bundle_id).manifest',
              ],
              'action': [
                # Use plutil -convert xml1 to put the plist into Apple's
                # canonical format. As a side effect, this ensures that the
                # plist is well-formed.
                'plutil',
                '-convert',
                'xml1',
                '<@(_inputs)',
                '-o',
                '<@(_outputs)',
              ],
              'message':
                  'Copying the MCX policy manifest file to the manifest bundle',
              'process_outputs_as_mac_bundle_resources': 1,
            },
            {
              'action_name':
                  'Copy Localizable.strings files to manifest bundle',
              'variables': {
                'input_path': '<(grit_out_dir)/app/policy/mac/strings',
                # Directory to collect the Localizable.strings files before
                # they are copied to the bundle.
                'output_path': '<(INTERMEDIATE_DIR)/app_manifest',
                # The reason we are not enumerating all the locales is that
                # the translations would eat up 3.5MB disk space in the
                # application bundle:
                'available_locales': 'en',
              },
              'inputs': [
                # TODO: remove this helper when we have loops in GYP
                '>!@(<(apply_locales_cmd) -d \'<(input_path)/ZZLOCALE.lproj/Localizable.strings\' <(available_locales))',
              ],
              'outputs': [
                # TODO: remove this helper when we have loops in GYP
                '>!@(<(apply_locales_cmd) -d \'<(output_path)/ZZLOCALE.lproj/Localizable.strings\' <(available_locales))',
              ],
              'action': [
                'cp', '-R',
                '<(input_path)/',
                '<(output_path)',
              ],
              'message':
                  'Copy the Localizable.strings files to the manifest bundle',
              'process_outputs_as_mac_bundle_resources': 1,
            },
          ],
        },
      ],
    }],
  ],
}
