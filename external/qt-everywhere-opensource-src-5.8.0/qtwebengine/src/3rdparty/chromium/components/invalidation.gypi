# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/invalidation/public
      'target_name': 'invalidation_public',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation',
        # TODO(akalin): Remove this (http://crbug.com/133352).
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_proto_cpp',
      ],
      'export_dependent_settings': [
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'invalidation/public/ack_handle.cc',
        'invalidation/public/ack_handle.h',
        'invalidation/public/ack_handler.cc',
        'invalidation/public/ack_handler.h',
        'invalidation/public/invalidation.cc',
        'invalidation/public/invalidation.h',
        'invalidation/public/invalidation_export.h',
        'invalidation/public/invalidation_handler.cc',
        'invalidation/public/invalidation_handler.h',
        'invalidation/public/invalidation_service.h',
        'invalidation/public/invalidation_util.cc',
        'invalidation/public/invalidation_util.h',
        'invalidation/public/invalidator_state.cc',
        'invalidation/public/invalidator_state.h',
        'invalidation/public/object_id_invalidation_map.cc',
        'invalidation/public/object_id_invalidation_map.h',
        'invalidation/public/single_object_invalidation_set.cc',
        'invalidation/public/single_object_invalidation_set.h',
      ],
    },
    {
      # GN version: //components/invalidation/impl
      'target_name': 'invalidation_impl',
      'type': 'static_library',
      'dependencies': [
        'invalidation_public',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../google_apis/google_apis.gyp:google_apis',
        '../jingle/jingle.gyp:notifier',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_proto_cpp',
        'data_use_measurement_core',
        'gcm_driver',
        'keyed_service_core',
        'pref_registry',
        'prefs/prefs.gyp:prefs',
        'signin_core_browser',
      ],
      'export_dependent_settings': [
        'invalidation_public',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'invalidation/impl/invalidation_logger.cc',
        'invalidation/impl/invalidation_logger.h',
        'invalidation/impl/invalidation_logger_observer.h',
        'invalidation/impl/invalidation_prefs.cc',
        'invalidation/impl/invalidation_prefs.h',
        'invalidation/impl/invalidation_service_util.cc',
        'invalidation/impl/invalidation_service_util.h',
        'invalidation/impl/invalidation_state_tracker.cc',
        'invalidation/impl/invalidation_state_tracker.h',
        'invalidation/impl/invalidation_switches.cc',
        'invalidation/impl/invalidation_switches.h',
        'invalidation/impl/invalidator.cc',
        'invalidation/impl/invalidator.h',
        'invalidation/impl/invalidator_registrar.cc',
        'invalidation/impl/invalidator_registrar.h',
        'invalidation/impl/invalidator_storage.cc',
        'invalidation/impl/invalidator_storage.h',
        'invalidation/impl/mock_ack_handler.cc',
        'invalidation/impl/mock_ack_handler.h',
        'invalidation/impl/profile_invalidation_provider.cc',
        'invalidation/impl/profile_invalidation_provider.h',
        'invalidation/impl/unacked_invalidation_set.cc',
        'invalidation/impl/unacked_invalidation_set.h',
      ],
      'conditions': [
          ['OS != "android"', {
            'sources': [
              # Note: sources list duplicated in GN build.
              'invalidation/impl/gcm_invalidation_bridge.cc',
              'invalidation/impl/gcm_invalidation_bridge.h',
              'invalidation/impl/gcm_network_channel.cc',
              'invalidation/impl/gcm_network_channel.h',
              'invalidation/impl/gcm_network_channel_delegate.h',
              'invalidation/impl/invalidation_notifier.cc',
              'invalidation/impl/invalidation_notifier.h',
              'invalidation/impl/non_blocking_invalidator.cc',
              'invalidation/impl/non_blocking_invalidator.h',
              'invalidation/impl/notifier_reason_util.cc',
              'invalidation/impl/notifier_reason_util.h',
              'invalidation/impl/p2p_invalidator.cc',
              'invalidation/impl/p2p_invalidator.h',
              'invalidation/impl/push_client_channel.cc',
              'invalidation/impl/push_client_channel.h',
              'invalidation/impl/registration_manager.cc',
              'invalidation/impl/registration_manager.h',
              'invalidation/impl/state_writer.h',
              'invalidation/impl/sync_invalidation_listener.cc',
              'invalidation/impl/sync_invalidation_listener.h',
              'invalidation/impl/sync_system_resources.cc',
              'invalidation/impl/sync_system_resources.h',
              'invalidation/impl/ticl_invalidation_service.cc',
              'invalidation/impl/ticl_invalidation_service.h',
              'invalidation/impl/ticl_profile_settings_provider.cc',
              'invalidation/impl/ticl_profile_settings_provider.h',
              'invalidation/impl/ticl_settings_provider.cc',
              'invalidation/impl/ticl_settings_provider.h',
            ],
          }],
        ['OS == "android"', {
          'dependencies': [
            'invalidation_jni_headers',
          ],
          'sources': [
            'invalidation/impl/android/component_jni_registrar.cc',
            'invalidation/impl/android/component_jni_registrar.h',
            'invalidation/impl/invalidation_service_android.cc',
            'invalidation/impl/invalidation_service_android.h',
          ],
        }],
      ],
    },
    {
      # GN version: //components/invalidation:test_support
      'target_name': 'invalidation_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../google_apis/google_apis.gyp:google_apis',
        '../jingle/jingle.gyp:notifier',
        '../jingle/jingle.gyp:notifier_test_util',
        '../net/net.gyp:net',
        '../testing/gmock.gyp:gmock',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_proto_cpp',
        'gcm_driver_test_support',
        'keyed_service_core',
      ],
      'export_dependent_settings': [
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_proto_cpp',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'invalidation/impl/fake_invalidation_handler.cc',
        'invalidation/impl/fake_invalidation_handler.h',
        'invalidation/impl/fake_invalidation_service.cc',
        'invalidation/impl/fake_invalidation_service.h',
        'invalidation/impl/fake_invalidation_state_tracker.cc',
        'invalidation/impl/fake_invalidation_state_tracker.h',
        'invalidation/impl/fake_invalidator.cc',
        'invalidation/impl/fake_invalidator.h',
        'invalidation/impl/invalidation_service_test_template.cc',
        'invalidation/impl/invalidation_service_test_template.h',
        'invalidation/impl/invalidation_test_util.cc',
        'invalidation/impl/invalidation_test_util.h',
        'invalidation/impl/invalidator_test_template.cc',
        'invalidation/impl/invalidator_test_template.h',
        'invalidation/impl/object_id_invalidation_map_test_util.cc',
        'invalidation/impl/object_id_invalidation_map_test_util.h',
        'invalidation/impl/unacked_invalidation_set_test_util.cc',
        'invalidation/impl/unacked_invalidation_set_test_util.h',
      ],
      'conditions': [
          ['OS != "android"', {
            'sources': [
              # Note: sources list duplicated in GN build.
              'invalidation/impl/p2p_invalidation_service.cc',
              'invalidation/impl/p2p_invalidation_service.h',
            ],
          }],
          ['OS == "android"', {
            'dependencies': [
              'invalidation_jni_headers',
            ],
          }],
      ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'invalidation_java',
          'type': 'none',
          'dependencies': [
            'invalidation_proto_java',
            '../base/base.gyp:base',
            '../sync/sync.gyp:sync_java',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_javalib',
          ],
          'variables': {
            'java_in_dir': 'invalidation/impl/android/java',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'invalidation_proto_java',
          'type': 'none',
          'sources': [
            'invalidation/impl/android/proto/serialized_invalidation.proto',
          ],
          'includes': [ '../build/protoc_java.gypi' ],
        },
        {
          'target_name': 'invalidation_javatests',
          'type': 'none',
          'dependencies': [
            'invalidation_java',
            '../base/base.gyp:base_java_test_support',
            '../content/content_shell_and_tests.gyp:content_java_test_support',
          ],
          'variables': {
            'java_in_dir': 'invalidation/impl/android/javatests',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'invalidation_jni_headers',
          'type': 'none',
          'sources': [
            'invalidation/impl/android/java/src/org/chromium/components/invalidation/InvalidationService.java',
          ],
          'variables': {
            'jni_gen_package': 'components/invalidation',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          'target_name': 'components_invalidation_impl_junit_tests',
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
     },
    ],
  ],
}
