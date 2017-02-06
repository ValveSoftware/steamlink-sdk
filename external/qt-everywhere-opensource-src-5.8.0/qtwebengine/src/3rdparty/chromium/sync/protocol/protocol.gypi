# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # The directory that contains the sync protocol buffer definitions.
    # This variable can be overridden for other builds that require the proto
    # files to be stored in an intermediate directory.
    'sync_proto_sources_dir%': '.',

    # The list of sync protocol buffer definitions.
    'sync_proto_source_paths': [
      '<(sync_proto_sources_dir)/app_notification_specifics.proto',
      '<(sync_proto_sources_dir)/app_setting_specifics.proto',
      '<(sync_proto_sources_dir)/app_specifics.proto',
      '<(sync_proto_sources_dir)/app_list_specifics.proto',
      '<(sync_proto_sources_dir)/arc_package_specifics.proto',
      '<(sync_proto_sources_dir)/article_specifics.proto',
      '<(sync_proto_sources_dir)/attachments.proto',
      '<(sync_proto_sources_dir)/autofill_specifics.proto',
      '<(sync_proto_sources_dir)/bookmark_specifics.proto',
      '<(sync_proto_sources_dir)/client_commands.proto',
      '<(sync_proto_sources_dir)/client_debug_info.proto',
      '<(sync_proto_sources_dir)/data_type_state.proto',
      '<(sync_proto_sources_dir)/device_info_specifics.proto',
      '<(sync_proto_sources_dir)/dictionary_specifics.proto',
      '<(sync_proto_sources_dir)/encryption.proto',
      '<(sync_proto_sources_dir)/entity_metadata.proto',
      '<(sync_proto_sources_dir)/experiment_status.proto',
      '<(sync_proto_sources_dir)/experiments_specifics.proto',
      '<(sync_proto_sources_dir)/extension_setting_specifics.proto',
      '<(sync_proto_sources_dir)/extension_specifics.proto',
      '<(sync_proto_sources_dir)/favicon_image_specifics.proto',
      '<(sync_proto_sources_dir)/favicon_tracking_specifics.proto',
      '<(sync_proto_sources_dir)/get_updates_caller_info.proto',
      '<(sync_proto_sources_dir)/history_delete_directive_specifics.proto',
      '<(sync_proto_sources_dir)/history_status.proto',
      '<(sync_proto_sources_dir)/nigori_specifics.proto',
      '<(sync_proto_sources_dir)/managed_user_setting_specifics.proto',
      '<(sync_proto_sources_dir)/managed_user_shared_setting_specifics.proto',
      '<(sync_proto_sources_dir)/managed_user_specifics.proto',
      '<(sync_proto_sources_dir)/managed_user_whitelist_specifics.proto',
      '<(sync_proto_sources_dir)/password_specifics.proto',
      '<(sync_proto_sources_dir)/preference_specifics.proto',
      '<(sync_proto_sources_dir)/priority_preference_specifics.proto',
      '<(sync_proto_sources_dir)/search_engine_specifics.proto',
      '<(sync_proto_sources_dir)/session_specifics.proto',
      '<(sync_proto_sources_dir)/sync.proto',
      '<(sync_proto_sources_dir)/sync_enums.proto',
      '<(sync_proto_sources_dir)/synced_notification_app_info_specifics.proto',
      '<(sync_proto_sources_dir)/synced_notification_specifics.proto',
      '<(sync_proto_sources_dir)/test.proto',
      '<(sync_proto_sources_dir)/theme_specifics.proto',
      '<(sync_proto_sources_dir)/typed_url_specifics.proto',
      '<(sync_proto_sources_dir)/unique_position.proto',
      '<(sync_proto_sources_dir)/wifi_credential_specifics.proto',
    ],
  },
}
