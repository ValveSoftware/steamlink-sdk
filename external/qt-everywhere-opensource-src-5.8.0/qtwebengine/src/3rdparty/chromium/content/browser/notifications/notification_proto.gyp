{
  'targets': [
    {
      # GN version: //content/browser/notifications:notification_proto
      'target_name': 'notification_proto',
      'type': 'static_library',
      'sources': [
        'notification_database_data.proto',
      ],
      'variables': {
        'proto_in_dir': '.',
        'proto_out_dir': 'content/browser/notifications',
      },
      'includes': [ '../../../build/protoc.gypi' ]
    },
  ],
}
