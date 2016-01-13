# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },

  'targets': [
    # The public GCM target.
    {
      'target_name': 'gcm',
      'type': '<(component)',
      'variables': {
        'enable_wexit_time_destructors': 1,
        'proto_in_dir': './protocol',
        'proto_out_dir': 'google_apis/gcm/protocol',
        'cc_generator_options': 'dllexport_decl=GCM_EXPORT:',
        'cc_include': 'google_apis/gcm/base/gcm_export.h',
      },
      'include_dirs': [
        '../..',
      ],
      'defines': [
        'GCM_IMPLEMENTATION',
      ],
      'export_dependent_settings': [
        '../../third_party/protobuf/protobuf.gyp:protobuf_lite'
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../net/net.gyp:net',
        '../../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        '../../third_party/protobuf/protobuf.gyp:protobuf_lite',
        '../../url/url.gyp:url_lib',
      ],
      'sources': [
        'base/mcs_message.cc',
        'base/mcs_message.h',
        'base/mcs_util.cc',
        'base/mcs_util.h',
        'base/socket_stream.cc',
        'base/socket_stream.h',
        'engine/checkin_request.cc',
        'engine/checkin_request.h',
        'engine/connection_factory.cc',
        'engine/connection_factory.h',
        'engine/connection_factory_impl.cc',
        'engine/connection_factory_impl.h',
        'engine/connection_handler.cc',
        'engine/connection_handler.h',
        'engine/connection_handler_impl.cc',
        'engine/connection_handler_impl.h',
        'engine/gcm_store.cc',
        'engine/gcm_store.h',
        'engine/gcm_store_impl.cc',
        'engine/gcm_store_impl.h',
        'engine/gservices_settings.cc',
        'engine/gservices_settings.h',
        'engine/heartbeat_manager.cc',
        'engine/heartbeat_manager.h',
        'engine/mcs_client.cc',
        'engine/mcs_client.h',
        'engine/registration_info.cc',
        'engine/registration_info.h',
        'engine/registration_request.cc',
        'engine/registration_request.h',
        'engine/unregistration_request.cc',
        'engine/unregistration_request.h',
        'monitoring/gcm_stats_recorder.h',
        'protocol/android_checkin.proto',
        'protocol/checkin.proto',
        'protocol/mcs.proto',
      ],
      'includes': [
        '../../build/protoc.gypi'
      ],
    },

    # The test support library that is needed to test gcm.
    {
      'target_name': 'gcm_test_support',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'export_dependent_settings': [
        '../../third_party/protobuf/protobuf.gyp:protobuf_lite'
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../testing/gtest.gyp:gtest',
        '../../third_party/protobuf/protobuf.gyp:protobuf_lite',
        'gcm',
      ],
      'sources': [
        'base/fake_encryptor.cc',
        'base/fake_encryptor.h',
        'engine/fake_connection_factory.cc',
        'engine/fake_connection_factory.h',
        'engine/fake_connection_handler.cc',
        'engine/fake_connection_handler.h',
        'monitoring/fake_gcm_stats_recorder.cc',
        'monitoring/fake_gcm_stats_recorder.h',
      ],
    },    

    # A standalone MCS (mobile connection server) client.
    {
      'target_name': 'mcs_probe',
      'type': 'executable',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'include_dirs': [
        '../..',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../net/net.gyp:net',
        '../../net/net.gyp:net_test_support',
        '../../third_party/protobuf/protobuf.gyp:protobuf_lite',
        'gcm',
        'gcm_test_support'
      ],
      'sources': [
        'tools/mcs_probe.cc',
      ],
    },

    # The main GCM unit tests.
    {
      'target_name': 'gcm_unit_tests',
      'type': '<(gtest_target_type)',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'include_dirs': [
        '../..',
      ],
      'export_dependent_settings': [
        '../../third_party/protobuf/protobuf.gyp:protobuf_lite'
      ],
      'dependencies': [
        '../../base/base.gyp:run_all_unittests',
        '../../base/base.gyp:base',
        '../../net/net.gyp:net',
        '../../net/net.gyp:net_test_support',
        '../../testing/gtest.gyp:gtest',
        '../../third_party/protobuf/protobuf.gyp:protobuf_lite',
        'mcs_probe',
        'gcm',
        'gcm_test_support'
      ],
      'sources': [
        'base/mcs_message_unittest.cc',
        'base/mcs_util_unittest.cc',
        'base/socket_stream_unittest.cc',
        'engine/checkin_request_unittest.cc',
        'engine/connection_factory_impl_unittest.cc',
        'engine/connection_handler_impl_unittest.cc',
        'engine/gcm_store_impl_unittest.cc',
        'engine/gservices_settings_unittest.cc',
        'engine/heartbeat_manager_unittest.cc',
        'engine/mcs_client_unittest.cc',
        'engine/registration_request_unittest.cc',
        'engine/unregistration_request_unittest.cc',
      ]
    },
  ],
}
