# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/drive:drive
      'target_name': 'drive',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'drive_proto',
        '../base/base.gyp:base',
        '../components/components.gyp:invalidation_public',

        # TODO(lukasza): Remove this dependency (see DEPS file for more info).
        '../content/content.gyp:content_browser',

        '../google_apis/google_apis.gyp:google_apis',
        '../net/net.gyp:net',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_proto_cpp',
        '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        '../third_party/re2/re2.gyp:re2',
      ],
      'sources': [
        'drive/drive_api_util.cc',
        'drive/drive_api_util.h',
        'drive/drive_app_registry.cc',
        'drive/drive_app_registry.h',
        'drive/drive_app_registry_observer.h',
        'drive/drive_notification_manager.cc',
        'drive/drive_notification_manager.h',
        'drive/drive_notification_observer.h',
        'drive/drive_pref_names.cc',
        'drive/drive_pref_names.h',
        'drive/drive_uploader.cc',
        'drive/drive_uploader.h',
        'drive/event_logger.cc',
        'drive/event_logger.h',
        'drive/file_change.cc',
        'drive/file_change.h',
        'drive/file_errors.cc',
        'drive/file_errors.h',
        'drive/file_system_core_util.cc',
        'drive/file_system_core_util.h',
        'drive/file_system_metadata.cc',
        'drive/file_system_metadata.h',
        'drive/file_write_watcher.cc',
        'drive/file_write_watcher.h',
        'drive/job_list.cc',
        'drive/job_list.h',
        'drive/job_queue.cc',
        'drive/job_queue.h',
        'drive/job_scheduler.cc',
        'drive/job_scheduler.h',
        'drive/local_file_reader.cc',
        'drive/local_file_reader.h',
        'drive/resource_entry_conversion.cc',
        'drive/resource_entry_conversion.h',
        'drive/resource_metadata_storage.cc',
        'drive/resource_metadata_storage.h',
        'drive/service/drive_api_service.cc',
        'drive/service/drive_api_service.h',
        'drive/service/drive_service_interface.cc',
        'drive/service/drive_service_interface.h',
      ],
    },

    {
      # GN version: //components/drive:proto
      # Protobuf compiler / generator for the Drive protocol buffer.
      'target_name': 'drive_proto',
      'type': 'static_library',
      'sources': [ 'drive/drive.proto' ],
      'variables': {
        'proto_in_dir': 'drive',
        'proto_out_dir': 'components/drive',
      },
      'includes': [ '../build/protoc.gypi' ]
    },

    {
      # GN version: //components/drive:test_support
      'target_name': 'drive_test_support',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'drive',
        'drive_proto',
        '../base/base.gyp:base',
        '../content/content_shell_and_tests.gyp:test_support_content',
        '../google_apis/google_apis.gyp:google_apis',
        '../net/net.gyp:net',
      ],
      'sources': [
        "drive/service/dummy_drive_service.cc",
        "drive/service/dummy_drive_service.h",
        "drive/service/fake_drive_service.cc",
        "drive/service/fake_drive_service.h",
        "drive/service/test_util.cc",
        "drive/service/test_util.h",
      ],
    },

    # TODO(lukasza): drive_unittests target.
    # Currently tests are built as part of chrome/chrome_tests_unit.gypi.
  ],

  'conditions': [
    ['chromeos==1', {
      'targets': [
        {
          # GN version: //components/drive:drive_chromeos
          'target_name': 'drive_chromeos',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            'drive',
	    'drive_proto',
            '../base/base.gyp:base',
            '../google_apis/google_apis.gyp:google_apis',
            '../net/net.gyp:net',
          ],
          'sources': [
            'drive/chromeos/change_list_loader.cc',
            'drive/chromeos/change_list_loader.h',
            'drive/chromeos/change_list_loader_observer.h',
            'drive/chromeos/change_list_processor.cc',
            'drive/chromeos/change_list_processor.h',
            'drive/chromeos/directory_loader.cc',
            'drive/chromeos/directory_loader.h',
            'drive/chromeos/file_cache.cc',
            'drive/chromeos/file_cache.h',
            'drive/chromeos/file_system.cc',
            'drive/chromeos/file_system.h',
            'drive/chromeos/file_system/copy_operation.cc',
            'drive/chromeos/file_system/copy_operation.h',
            'drive/chromeos/file_system/create_directory_operation.cc',
            'drive/chromeos/file_system/create_directory_operation.h',
            'drive/chromeos/file_system/create_file_operation.cc',
            'drive/chromeos/file_system/create_file_operation.h',
            'drive/chromeos/file_system/download_operation.cc',
            'drive/chromeos/file_system/download_operation.h',
            'drive/chromeos/file_system/get_file_for_saving_operation.cc',
            'drive/chromeos/file_system/get_file_for_saving_operation.h',
            'drive/chromeos/file_system/move_operation.cc',
            'drive/chromeos/file_system/move_operation.h',
            'drive/chromeos/file_system/open_file_operation.cc',
            'drive/chromeos/file_system/open_file_operation.h',
            'drive/chromeos/file_system/operation_delegate.cc',
            'drive/chromeos/file_system/operation_delegate.h',
            'drive/chromeos/file_system/remove_operation.cc',
            'drive/chromeos/file_system/remove_operation.h',
            'drive/chromeos/file_system/search_operation.cc',
            'drive/chromeos/file_system/search_operation.h',
            'drive/chromeos/file_system/set_property_operation.cc',
            'drive/chromeos/file_system/set_property_operation.h',
            'drive/chromeos/file_system/touch_operation.cc',
            'drive/chromeos/file_system/touch_operation.h',
            'drive/chromeos/file_system/truncate_operation.cc',
            'drive/chromeos/file_system/truncate_operation.h',
            'drive/chromeos/file_system_interface.cc',
            'drive/chromeos/file_system_interface.h',
            'drive/chromeos/file_system_observer.h',
            'drive/chromeos/remove_stale_cache_files.cc',
            'drive/chromeos/remove_stale_cache_files.h',
            'drive/chromeos/resource_metadata.cc',
            'drive/chromeos/resource_metadata.h',
            'drive/chromeos/search_metadata.cc',
            'drive/chromeos/search_metadata.h',
            'drive/chromeos/sync/entry_revert_performer.cc',
            'drive/chromeos/sync/entry_revert_performer.h',
            'drive/chromeos/sync/entry_update_performer.cc',
            'drive/chromeos/sync/entry_update_performer.h',
            'drive/chromeos/sync/remove_performer.cc',
            'drive/chromeos/sync/remove_performer.h',
            'drive/chromeos/sync_client.cc',
            'drive/chromeos/sync_client.h',
          ],
        },

        {
          # GN version: //components/drive:test_support_chromeos
          'target_name': 'drive_test_support_chromeos',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            'drive',
            'drive_chromeos',
            'drive_proto',
            '../base/base.gyp:base',
            '../content/content_shell_and_tests.gyp:test_support_content',
          ],
          'sources': [
            "drive/chromeos/drive_test_util.cc",
            "drive/chromeos/drive_test_util.h",
            "drive/chromeos/dummy_file_system.cc",
            "drive/chromeos/dummy_file_system.h",
            "drive/chromeos/fake_file_system.cc",
            "drive/chromeos/fake_file_system.h",
            "drive/chromeos/fake_free_disk_space_getter.cc",
            "drive/chromeos/fake_free_disk_space_getter.h",
          ]
        }
      ]
    }]
  ]
}
