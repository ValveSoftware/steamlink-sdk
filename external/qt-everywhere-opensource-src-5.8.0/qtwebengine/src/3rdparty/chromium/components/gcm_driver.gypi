# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/gcm_driver/common
      'target_name': 'gcm_driver_common',
      'type': '<(component)',
      'include_dirs': [
        '..',
      ],
      'defines': [
        'GCM_DRIVER_IMPLEMENTATION',
      ],
      'sources': [
         # Note: file list duplicated in GN build.
        'gcm_driver/common/gcm_driver_export.h',
        'gcm_driver/common/gcm_messages.cc',
        'gcm_driver/common/gcm_messages.h',
      ],
    },
    {
      # GN version: //components/gcm_driver
      'target_name': 'gcm_driver',
      'type': 'static_library',
      'dependencies': [
        'gcm_driver_common',
        'gcm_driver_crypto',
        'os_crypt',
        'sync_driver',
        '../base/base.gyp:base',
        '../google_apis/gcm/gcm.gyp:gcm',
        '../net/net.gyp:net',
        '../sync/sync.gyp:sync_proto',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
         # Note: file list duplicated in GN build.
        'gcm_driver/android/component_jni_registrar.cc',
        'gcm_driver/android/component_jni_registrar.h',
        'gcm_driver/default_gcm_app_handler.cc',
        'gcm_driver/default_gcm_app_handler.h',
        'gcm_driver/gcm_account_mapper.cc',
        'gcm_driver/gcm_account_mapper.h',
        'gcm_driver/gcm_account_tracker.cc',
        'gcm_driver/gcm_account_tracker.h',
        'gcm_driver/gcm_activity.cc',
        'gcm_driver/gcm_activity.h',
        'gcm_driver/gcm_app_handler.cc',
        'gcm_driver/gcm_app_handler.h',
        'gcm_driver/gcm_backoff_policy.cc',
        'gcm_driver/gcm_backoff_policy.h',
        'gcm_driver/gcm_channel_status_request.cc',
        'gcm_driver/gcm_channel_status_request.h',
        'gcm_driver/gcm_channel_status_syncer.cc',
        'gcm_driver/gcm_channel_status_syncer.h',
        'gcm_driver/gcm_client.cc',
        'gcm_driver/gcm_client.h',
        'gcm_driver/gcm_client_factory.cc',
        'gcm_driver/gcm_client_factory.h',
        'gcm_driver/gcm_client_impl.cc',
        'gcm_driver/gcm_client_impl.h',
        'gcm_driver/gcm_connection_observer.cc',
        'gcm_driver/gcm_connection_observer.h',
        'gcm_driver/gcm_delayed_task_controller.cc',
        'gcm_driver/gcm_delayed_task_controller.h',
        'gcm_driver/gcm_desktop_utils.cc',
        'gcm_driver/gcm_desktop_utils.h',
        'gcm_driver/gcm_driver.cc',
        'gcm_driver/gcm_driver.h',
        'gcm_driver/gcm_driver_android.cc',
        'gcm_driver/gcm_driver_android.h',
        'gcm_driver/gcm_driver_constants.cc',
        'gcm_driver/gcm_driver_constants.h',
        'gcm_driver/gcm_driver_desktop.cc',
        'gcm_driver/gcm_driver_desktop.h',
        'gcm_driver/gcm_internals_constants.cc',
        'gcm_driver/gcm_internals_constants.h',
        'gcm_driver/gcm_internals_helper.cc',
        'gcm_driver/gcm_internals_helper.h',
        'gcm_driver/gcm_profile_service.cc',
        'gcm_driver/gcm_profile_service.h',
        'gcm_driver/gcm_stats_recorder_android.cc',
        'gcm_driver/gcm_stats_recorder_android.h',
        'gcm_driver/gcm_stats_recorder_impl.cc',
        'gcm_driver/gcm_stats_recorder_impl.h',
        'gcm_driver/registration_info.cc',
        'gcm_driver/registration_info.h',
        'gcm_driver/system_encryptor.cc',
        'gcm_driver/system_encryptor.h',
      ],
      'conditions': [
        ['OS == "android"', {
          'dependencies': [
            'gcm_driver_jni_headers',
          ],
          'dependencies!': [
            '../google_apis/gcm/gcm.gyp:gcm',
          ],
          'sources!': [
            'gcm_driver/gcm_account_mapper.cc',
            'gcm_driver/gcm_account_mapper.h',
            'gcm_driver/gcm_channel_status_request.cc',
            'gcm_driver/gcm_channel_status_request.h',
            'gcm_driver/gcm_channel_status_syncer.cc',
            'gcm_driver/gcm_channel_status_syncer.h',
            'gcm_driver/gcm_client_factory.cc',
            'gcm_driver/gcm_client_factory.h',
            'gcm_driver/gcm_client_impl.cc',
            'gcm_driver/gcm_client_impl.h',
            'gcm_driver/gcm_desktop_utils.cc',
            'gcm_driver/gcm_desktop_utils.h',
            'gcm_driver/gcm_driver_desktop.cc',
            'gcm_driver/gcm_driver_desktop.h',
            'gcm_driver/gcm_stats_recorder_impl.cc',
            'gcm_driver/gcm_stats_recorder_impl.h',
          ],
        }],
        ['chromeos == 1', {
          'dependencies': [
            'timers',
          ],
        }],
      ],
    },
    {
      # GN version: //components/gcm_driver:test_support
      'target_name': 'gcm_driver_test_support',
      'type': 'static_library',
      'dependencies': [
        'gcm_driver',
        '../base/base.gyp:base',
        '../google_apis/gcm/gcm.gyp:gcm_test_support',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'gcm_driver/fake_gcm_app_handler.cc',
        'gcm_driver/fake_gcm_app_handler.h',
        'gcm_driver/fake_gcm_client.cc',
        'gcm_driver/fake_gcm_client.h',
        'gcm_driver/fake_gcm_client_factory.cc',
        'gcm_driver/fake_gcm_client_factory.h',
        'gcm_driver/fake_gcm_driver.cc',
        'gcm_driver/fake_gcm_driver.h',
      ],
      'conditions': [
        ['OS == "android"', {
          'dependencies!': [
            '../google_apis/gcm/gcm.gyp:gcm_test_support',
          ],
          'sources!': [
            'gcm_driver/fake_gcm_client.cc',
            'gcm_driver/fake_gcm_client.h',
            'gcm_driver/fake_gcm_client_factory.cc',
            'gcm_driver/fake_gcm_client_factory.h',
          ],
        }],
      ],
    },
    {
      # GN version: //components/gcm_driver/instance_id
      'target_name': 'instance_id',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'gcm_driver/instance_id/android/component_jni_registrar.cc',
        'gcm_driver/instance_id/android/component_jni_registrar.h',
        'gcm_driver/instance_id/instance_id.cc',
        'gcm_driver/instance_id/instance_id.h',
        'gcm_driver/instance_id/instance_id_android.cc',
        'gcm_driver/instance_id/instance_id_android.h',
        'gcm_driver/instance_id/instance_id_driver.cc',
        'gcm_driver/instance_id/instance_id_driver.h',
        'gcm_driver/instance_id/instance_id_impl.cc',
        'gcm_driver/instance_id/instance_id_impl.h',
      ],
      'conditions': [
        ['OS == "android"', {
          'dependencies': [
            'instance_id_driver_jni_headers',
          ],
          'sources!': [
            'gcm_driver/instance_id/instance_id_impl.cc',
            'gcm_driver/instance_id/instance_id_impl.h',
          ],
        }],
      ],
    },
    {
      # GN version: //components/gcm_driver/instance_id:test_support
      'target_name': 'instance_id_test_support',
      'type': 'static_library',
      'dependencies': [
        'gcm_driver_test_support',
        'instance_id',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'gcm_driver/instance_id/fake_gcm_driver_for_instance_id.cc',
        'gcm_driver/instance_id/fake_gcm_driver_for_instance_id.h',
        'gcm_driver/instance_id/scoped_use_fake_instance_id_android.cc',
        'gcm_driver/instance_id/scoped_use_fake_instance_id_android.h',
      ],
      'conditions': [
        ['OS == "android"', {
          'dependencies': [
            'instance_id_driver_test_support_jni_headers',
          ],
        }],
      ],
    },
    {
      # GN version: //components/gcm_driver/crypto
      'target_name': 'gcm_driver_crypto',
      'type': 'static_library',
      'dependencies': [
        'gcm_driver_crypto_proto',
        '../base/base.gyp:base',
        '../components/components.gyp:leveldb_proto',
        '../crypto/crypto.gyp:crypto',
        '../net/net.gyp:net',
        '../third_party/boringssl/boringssl.gyp:boringssl',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'gcm_driver/crypto/encryption_header_parsers.cc',
        'gcm_driver/crypto/encryption_header_parsers.h',
        'gcm_driver/crypto/gcm_encryption_provider.cc',
        'gcm_driver/crypto/gcm_encryption_provider.h',
        'gcm_driver/crypto/gcm_key_store.cc',
        'gcm_driver/crypto/gcm_key_store.h',
        'gcm_driver/crypto/gcm_message_cryptographer.cc',
        'gcm_driver/crypto/gcm_message_cryptographer.h',
        'gcm_driver/crypto/p256_key_util.cc',
        'gcm_driver/crypto/p256_key_util.h',
      ],
    },
    {
      # GN version: //components/gcm_driver/crypto/proto
      'target_name': 'gcm_driver_crypto_proto',
      'type': 'static_library',
      'sources': [
        'gcm_driver/crypto/proto/gcm_encryption_data.proto',
      ],
      'variables': {
        'proto_in_dir': 'gcm_driver/crypto/proto',
        'proto_out_dir': 'components/gcm_driver/crypto/proto',
      },
      'includes': [ '../build/protoc.gypi' ],
    },
    {
      # GN version: //components/gcm_driver/crypto:test_support
      'target_name': 'gcm_driver_crypto_test_support',
      'type': 'static_library',
      'dependencies': [
        'gcm_driver_common',
        'gcm_driver_crypto',
        '../base/base.gyp:base',
        '../crypto/crypto.gyp:crypto',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'gcm_driver/crypto/gcm_crypto_test_helpers.cc',
        'gcm_driver/crypto/gcm_crypto_test_helpers.h',
      ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          # GN version: //components/gcm_driver/android:gcm_driver_java
          'target_name': 'gcm_driver_java',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_java',
            '../content/content.gyp:content_java',
            '../sync/sync.gyp:sync_java',
          ],
          'variables': {
            'java_in_dir': 'gcm_driver/android/java',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          # GN version: //components/gcm_driver/android:jni_headers
          'target_name': 'gcm_driver_jni_headers',
          'type': 'none',
          'sources': [
            'gcm_driver/android/java/src/org/chromium/components/gcm_driver/GCMDriver.java',
          ],
          'variables': {
            'jni_gen_package': 'components/gcm_driver',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          # GN version: //components/gcm_driver/instance_id/android:instance_id_driver_java
          'target_name': 'instance_id_driver_java',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_java',
            '../third_party/android_tools/android_tools.gyp:google_play_services_javalib',
          ],
          'variables': {
            'java_in_dir': 'gcm_driver/instance_id/android/java',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          # GN version: //components/gcm_driver/instance_id/android:instance_id_driver_test_support_java
          'target_name': 'instance_id_driver_test_support_java',
          'type': 'none',
          'dependencies': [
            'instance_id_driver_java',
          ],
          'variables': {
            'java_in_dir': 'gcm_driver/instance_id/android/javatests',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          # GN version: //components/gcm_driver/instance_id/android:jni_headers
          'target_name': 'instance_id_driver_jni_headers',
          'type': 'none',
          'sources': [
            'gcm_driver/instance_id/android/java/src/org/chromium/components/gcm_driver/instance_id/InstanceIDBridge.java',
          ],
          'variables': {
            'jni_gen_package': 'components/gcm_driver/instance_id',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          # GN version: //components/gcm_driver/instance_id/android:test_support_jni_headers
          'target_name': 'instance_id_driver_test_support_jni_headers',
          'type': 'none',
          'sources': [
            'gcm_driver/instance_id/android/javatests/src/org/chromium/components/gcm_driver/instance_id/FakeInstanceIDWithSubtype.java',
          ],
          'variables': {
            'jni_gen_package': 'components/gcm_driver/instance_id',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
      ],
     },
    ],
  ],
}
