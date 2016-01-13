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
          'target_name': 'policy_component',
          'type': 'none',
          'dependencies': [
            'policy_component_common',
            'policy_component_browser',
          ],
        },
        {
          'target_name': 'policy_component_common',
          'type': 'static_library',
          'includes': [
            'policy/policy_common.gypi',
          ],
        },
        {
          'target_name': 'policy_component_browser',
          'type': 'static_library',
          'dependencies': [
            'policy_component_common',
          ],
          'includes': [
            'policy/policy_browser.gypi',
          ],
        },
      ],
    }, {  # component=="static_library"
      'targets': [
        {
          'target_name': 'policy_component',
          'type': 'shared_library',
          'includes': [
            'policy/policy_common.gypi',
            'policy/policy_browser.gypi',
          ],
        },
        {
          'target_name': 'policy_component_common',
          'type': 'none',
          'dependencies': [
            'policy_component',
          ],
        },
        {
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
          'target_name': 'cloud_policy_code_generate',
          'type': 'none',
          'actions': [
            {
              'inputs': [
                'policy/resources/policy_templates.json',
                '<(generate_policy_source_script_path)',
              ],
              'outputs': [
                '<(policy_constant_header_path)',
                '<(policy_constant_source_path)',
                '<(protobuf_decoder_path)',
                '<(chrome_settings_proto_path)',
                '<(cloud_policy_proto_path)',
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
                '<(OS)',
                '<(chromeos)',
                'policy/resources/policy_templates.json',
              ],
              'message': 'Generating policy source',
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
            'policy/core/common/mock_configuration_policy_provider.cc',
            'policy/core/common/mock_configuration_policy_provider.h',
            'policy/core/common/mock_policy_service.cc',
            'policy/core/common/mock_policy_service.h',
            'policy/core/common/policy_test_utils.cc',
            'policy/core/common/policy_test_utils.h',
            'policy/core/common/preferences_mock_mac.cc',
            'policy/core/common/preferences_mock_mac.h',
          ],
          'conditions': [
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
    ['OS=="win" and target_arch=="ia32" and configuration_policy==1', {
      'targets': [
        {
          'target_name': 'policy_win64',
          'type': 'static_library',
          'hard_dependency': 1,
          'sources': [
            '<(policy_constant_header_path)',
            '<(policy_constant_source_path)',
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
    ['OS=="win" or OS=="mac" or OS=="linux"', {
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
