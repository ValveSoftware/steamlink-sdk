# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # Protobuf compiler / generator for the MtpFileEntry and
      # MtpFileEntries protocol buffers.
      # GN version: //device/media_transfer_protocol:mtp_file_entry_proto
      'target_name': 'mtp_file_entry_proto',
      'type': 'static_library',
      'sources': [
        '../../third_party/cros_system_api/dbus/mtp_file_entry.proto',
      ],
      'variables': {
        'proto_in_dir': '../../third_party/cros_system_api/dbus',
        'proto_out_dir': 'device/media_transfer_protocol',
      },
      'includes': ['../../build/protoc.gypi'],
    },
    {
      # Protobuf compiler / generator for the MtpStorageInfo protocol
      # buffer.
      # GN version: //device/media_transfer_protocol:mtp_storage_info_proto
      'target_name': 'mtp_storage_info_proto',
      'type': 'static_library',
      'sources': [
        '../../third_party/cros_system_api/dbus/mtp_storage_info.proto',
      ],
      'variables': {
        'proto_in_dir': '../../third_party/cros_system_api/dbus',
        'proto_out_dir': 'device/media_transfer_protocol',
      },
      'includes': ['../../build/protoc.gypi'],
    },
    {
      # GN version: //device/media_transfer_protocol
      'target_name': 'device_media_transfer_protocol',
      'type': 'static_library',
      'dependencies': [
        '../../build/linux/system.gyp:dbus',
        '../../dbus/dbus.gyp:dbus',
        'mtp_file_entry_proto',
        'mtp_storage_info_proto',
      ],
      'sources': [
        'media_transfer_protocol_daemon_client.cc',
        'media_transfer_protocol_daemon_client.h',
        'media_transfer_protocol_manager.cc',
        'media_transfer_protocol_manager.h',
      ],
    },
  ],
}
