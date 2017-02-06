# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN: //components/offline_pages:offline_pages
      'target_name': 'offline_pages',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
        '../sql/sql.gyp:sql',
        'keyed_service_core',
      ],
      'sources': [
        'offline_pages/archive_manager.cc',
        'offline_pages/archive_manager.h',
        'offline_pages/client_namespace_constants.cc',
        'offline_pages/client_namespace_constants.h',
        'offline_pages/client_policy_controller.cc',
        'offline_pages/client_policy_controller.h',
        'offline_pages/offline_event_logger.cc',
        'offline_pages/offline_event_logger.h',
        'offline_pages/offline_page_archiver.h',
        'offline_pages/offline_page_client_policy.h',
        'offline_pages/offline_page_feature.cc',
        'offline_pages/offline_page_feature.h',
        'offline_pages/offline_page_item.cc',
        'offline_pages/offline_page_item.h',
        'offline_pages/offline_page_metadata_store.cc',
        'offline_pages/offline_page_metadata_store.h',
        'offline_pages/offline_page_metadata_store_sql.cc',
        'offline_pages/offline_page_metadata_store_sql.h',
        'offline_pages/offline_page_model.cc',
        'offline_pages/offline_page_model.h',
        'offline_pages/offline_page_model_event_logger.cc',
        'offline_pages/offline_page_model_event_logger.h',
        'offline_pages/offline_page_model_impl.cc',
        'offline_pages/offline_page_model_impl.h',
        'offline_pages/offline_page_storage_manager.cc',
        'offline_pages/offline_page_storage_manager.h',
        'offline_pages/offline_page_types.h',
        'offline_pages/snapshot_controller.cc',
        'offline_pages/snapshot_controller.h',
      ],
    },
    {
      # GN: //components/offline_pages/background:background_offliner
      'target_name': 'offline_pages_background_offliner',
      'type': 'static_library',
      'include_dirs': [
        '..',
        '../..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../sql/sql.gyp:sql',
        '../url/url.gyp:url_lib',
        'keyed_service_core',
      ],
      'sources': [
        'offline_pages/background/device_conditions.h',
        'offline_pages/background/offliner.h',
        'offline_pages/background/offliner_factory.h',
        'offline_pages/background/offliner_policy.h',
        'offline_pages/background/request_coordinator.cc',
        'offline_pages/background/request_coordinator.h',
        'offline_pages/background/request_coordinator_event_logger.cc',
        'offline_pages/background/request_coordinator_event_logger.h',
        'offline_pages/background/request_picker.cc',
        'offline_pages/background/request_picker.h',
        'offline_pages/background/request_queue.cc',
        'offline_pages/background/request_queue.h',
        'offline_pages/background/request_queue_in_memory_store.cc',
        'offline_pages/background/request_queue_in_memory_store.h',
        'offline_pages/background/request_queue_store.h',
        'offline_pages/background/request_queue_store_sql.cc',
        'offline_pages/background/request_queue_store_sql.h',
        'offline_pages/background/save_page_request.cc',
        'offline_pages/background/save_page_request.h',
        'offline_pages/background/scheduler.h',
      ],
    },
    {
      # GN version: //components/offline_pages:test_support
      'target_name': 'offline_pages_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        '../url/url.gyp:url_lib',
        'offline_pages',
      ],
      'sources': [
        'offline_pages/offline_page_test_archiver.cc',
        'offline_pages/offline_page_test_archiver.h',
        'offline_pages/offline_page_test_store.cc',
        'offline_pages/offline_page_test_store.h',
        'offline_pages/stub_offline_page_model.cc',
        'offline_pages/stub_offline_page_model.h',
      ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          # GN: //components/offline_pages:offline_page_model_enums_java
          'target_name': 'offline_page_model_enums_java',
          'type': 'none',
          'variables': {
            'source_file': 'offline_pages/offline_page_types.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
      ],
    }],
  ],
}
