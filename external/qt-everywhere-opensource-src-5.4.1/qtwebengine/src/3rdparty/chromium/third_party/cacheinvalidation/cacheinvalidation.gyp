# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # This library should build cleanly with the extra warnings turned on
    # for Chromium.
    'chromium_code': 1,
  },
  'targets': [
    # The C++ files generated from the cache invalidation protocol buffers.
    {
      'target_name': 'cacheinvalidation_proto_cpp',
      'type': 'static_library',
      'variables': {
        # The relative path of the cacheinvalidation proto files from this
        # gyp-file.
        # TODO(akalin): Add a RULE_INPUT_DIR predefined variable to gyp so
        # we don't need this variable.
        'proto_dir_relpath': 'google/cacheinvalidation',
        # Where files generated from proto files are put.
        'proto_in_dir': 'src/<(proto_dir_relpath)',
        'proto_out_dir': '<(proto_dir_relpath)',
      },
      'sources': [
        '<(proto_in_dir)/android_channel.proto',
        '<(proto_in_dir)/channel_common.proto',
        '<(proto_in_dir)/client.proto',
        '<(proto_in_dir)/client_gateway.proto',
        '<(proto_in_dir)/client_protocol.proto',
        '<(proto_in_dir)/client_test_internal.proto',
        '<(proto_in_dir)/types.proto',
      ],
      'includes': [ '../../build/protoc.gypi' ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(proto_out_dir)',
        ],
      },
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
      # channel_common.proto contains definition of ANDROID constant which on 
      # android build conflicts with compiler option -DANDROID. Remove protos
      # from android build.
      'conditions': [
        ['OS=="android"', {
          'sources!': [
            '<(proto_in_dir)/android_channel.proto',
            '<(proto_in_dir)/channel_common.proto',
          ],
        }],
      ],
    },
    # The main cache invalidation library.  External clients should depend
    # only on this.
    {
      'target_name': 'cacheinvalidation',
      'type': 'static_library',
      'sources': [
        'overrides/google/cacheinvalidation/deps/callback.h',
        'overrides/google/cacheinvalidation/deps/gmock.h',
        'overrides/google/cacheinvalidation/deps/googletest.h',
        'overrides/google/cacheinvalidation/deps/logging.h',
        'overrides/google/cacheinvalidation/deps/mutex.h',
        'overrides/google/cacheinvalidation/deps/random.h',
        'overrides/google/cacheinvalidation/deps/random.cc',
        'overrides/google/cacheinvalidation/deps/sha1-digest-function.h',
        'overrides/google/cacheinvalidation/deps/scoped_ptr.h',
        'overrides/google/cacheinvalidation/deps/stl-namespace.h',
        'overrides/google/cacheinvalidation/deps/string_util.h',
        'overrides/google/cacheinvalidation/deps/time.h',
        'src/google/cacheinvalidation/deps/digest-function.h',
        'src/google/cacheinvalidation/impl/basic-system-resources.cc',
        'src/google/cacheinvalidation/impl/basic-system-resources.h',
        'src/google/cacheinvalidation/impl/checking-invalidation-listener.cc',
        'src/google/cacheinvalidation/impl/checking-invalidation-listener.h',
        'src/google/cacheinvalidation/impl/client-protocol-namespace-fix.h',
        'src/google/cacheinvalidation/impl/constants.cc',
        'src/google/cacheinvalidation/impl/constants.h',
        'src/google/cacheinvalidation/impl/digest-store.h',
        'src/google/cacheinvalidation/impl/exponential-backoff-delay-generator.cc',
        'src/google/cacheinvalidation/impl/exponential-backoff-delay-generator.h',
        'src/google/cacheinvalidation/impl/invalidation-client-core.cc',
        'src/google/cacheinvalidation/impl/invalidation-client-core.h',
        'src/google/cacheinvalidation/impl/invalidation-client-factory.cc',
        'src/google/cacheinvalidation/impl/invalidation-client-impl.cc',
        'src/google/cacheinvalidation/impl/invalidation-client-impl.h',
        'src/google/cacheinvalidation/impl/invalidation-client-util.h',
        'src/google/cacheinvalidation/impl/log-macro.h',
        'src/google/cacheinvalidation/impl/object-id-digest-utils.cc',
        'src/google/cacheinvalidation/impl/object-id-digest-utils.h',
        'src/google/cacheinvalidation/impl/persistence-utils.cc',
        'src/google/cacheinvalidation/impl/persistence-utils.h',
        'src/google/cacheinvalidation/impl/proto-converter.cc',
        'src/google/cacheinvalidation/impl/proto-converter.h',
        'src/google/cacheinvalidation/impl/proto-helpers.h',
        'src/google/cacheinvalidation/impl/proto-helpers.cc',
        'src/google/cacheinvalidation/impl/protocol-handler.cc',
        'src/google/cacheinvalidation/impl/protocol-handler.h',
        'src/google/cacheinvalidation/impl/recurring-task.cc',
        'src/google/cacheinvalidation/impl/recurring-task.h',
        'src/google/cacheinvalidation/impl/registration-manager.cc',
        'src/google/cacheinvalidation/impl/registration-manager.h',
        'src/google/cacheinvalidation/impl/repeated-field-namespace-fix.h',
        'src/google/cacheinvalidation/impl/run-state.h',
        'src/google/cacheinvalidation/impl/safe-storage.cc',
        'src/google/cacheinvalidation/impl/safe-storage.h',
        'src/google/cacheinvalidation/impl/simple-registration-store.cc',
        'src/google/cacheinvalidation/impl/simple-registration-store.h',
        'src/google/cacheinvalidation/impl/smearer.h',
        'src/google/cacheinvalidation/impl/statistics.cc',
        'src/google/cacheinvalidation/impl/statistics.h',
        'src/google/cacheinvalidation/impl/throttle.cc',
        'src/google/cacheinvalidation/impl/throttle.h',
        'src/google/cacheinvalidation/impl/ticl-message-validator.cc',
        'src/google/cacheinvalidation/impl/ticl-message-validator.h',
        'src/google/cacheinvalidation/include/invalidation-client.h',
        'src/google/cacheinvalidation/include/invalidation-client-factory.h',
        'src/google/cacheinvalidation/include/invalidation-listener.h',
        'src/google/cacheinvalidation/include/system-resources.h',
        'src/google/cacheinvalidation/include/types.h',
      ],
      'include_dirs': [
        './overrides',
        './src',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        'cacheinvalidation_proto_cpp',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          './overrides',
          './src',
        ],
      },
      # We avoid including header files from
      # cacheinvalidation_proto_cpp in our public header files so we
      # don't need to export its settings.
      'export_dependent_settings': [
        '../../base/base.gyp:base',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
    # Unittests for the cache invalidation library.
    # TODO(ghc): Write native tests and include them here.
    {
      'target_name': 'cacheinvalidation_unittests',
      'type': 'executable',
      'sources': [
        'src/google/cacheinvalidation/test/deterministic-scheduler.cc',
        'src/google/cacheinvalidation/test/deterministic-scheduler.h',
        'src/google/cacheinvalidation/test/test-logger.cc',
        'src/google/cacheinvalidation/test/test-logger.h',
        'src/google/cacheinvalidation/test/test-utils.cc',
        'src/google/cacheinvalidation/test/test-utils.h',
        'src/google/cacheinvalidation/impl/invalidation-client-impl_test.cc',
        'src/google/cacheinvalidation/impl/protocol-handler_test.cc',
        'src/google/cacheinvalidation/impl/recurring-task_test.cc',
        'src/google/cacheinvalidation/impl/throttle_test.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:run_all_unittests',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        'cacheinvalidation',
        'cacheinvalidation_proto_cpp',
      ],
    },
  ],
  'conditions': [
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'cacheinvalidation_unittests_run',
          'type': 'none',
          'dependencies': [
            'cacheinvalidation_unittests',
          ],
          'includes': [
            '../../build/isolate.gypi',
            'cacheinvalidation_unittests.isolate',
          ],
          'sources': [
            'cacheinvalidation_unittests.isolate',
          ],
        },
      ],
    }],
    ['OS == "android"', {
      'variables': {
        'emma_never_instrument': 1,
      },
      'targets': [
        {
          'target_name': 'cacheinvalidation_proto_java',
          'type': 'none',
          'variables': {
            'proto_in_dir': '../../third_party/cacheinvalidation/src/proto',
          },
          'sources': [
            '<(proto_in_dir)/android_channel.proto',
            '<(proto_in_dir)/android_listener.proto',
            '<(proto_in_dir)/android_service.proto',
            '<(proto_in_dir)/android_state.proto',
            '<(proto_in_dir)/channel_common.proto',
            '<(proto_in_dir)/client.proto',
            '<(proto_in_dir)/client_protocol.proto',
            '<(proto_in_dir)/java_client.proto',
            '<(proto_in_dir)/types.proto',
          ],
          'includes': [ '../../build/protoc_java.gypi' ],
        },
        {
          'target_name': 'cacheinvalidation_example_proto_java',
          'type': 'none',
          'variables': {
            'cacheinvalidation_in_dir': '../../third_party/cacheinvalidation/src',
            'proto_in_dir' : '<(cacheinvalidation_in_dir)/java/com/google/ipc/invalidation/examples/android2',
          },
          'sources': [
            '<(proto_in_dir)/example_listener.proto',
          ],
          'includes': [ '../../build/protoc_java.gypi' ],
        },
        {
          'target_name': 'cacheinvalidation_javalib',
          'type': 'none',
          'dependencies': [
            '../../third_party/android_tools/android_tools.gyp:android_gcm',
            '../../third_party/guava/guava.gyp:guava_javalib',
            'cacheinvalidation_aidl_javalib',
            'cacheinvalidation_example_proto_java',
            'cacheinvalidation_proto_java',
          ],
          'variables': {
            'java_in_dir': '../../build/android/empty',
            'additional_src_dirs': [ 'src/java/' ],
          },
          'includes': [ '../../build/java.gypi' ],
        },
        {
          'target_name': 'cacheinvalidation_aidl_javalib',
          'type': 'none',
          'variables': {
            # TODO(shashishekhar): aidl_interface_file should be made optional.
            'aidl_interface_file':'<(android_sdk)/framework.aidl'
          },
          'sources': [
            'src/java/com/google/ipc/invalidation/external/client/android/service/InvalidationService.aidl',
            'src/java/com/google/ipc/invalidation/external/client/android/service/ListenerService.aidl',
            'src/java/com/google/ipc/invalidation/testing/android/InvalidationTest.aidl',
          ],
          'includes': [ '../../build/java_aidl.gypi' ],
        },
      ],
    }],
  ],
}
