{
  'targets': [
    {
      # GN version: //content/browser/service_worker:database_proto
      'target_name': 'database_proto',
      'type': 'static_library',
      'sources': [
        'service_worker_database.proto',
      ],
      'variables': {
        'proto_in_dir': '.',
        'proto_out_dir': 'content/browser/service_worker',
      },
      'includes': [ '../../../build/protoc.gypi' ]
    },
  ],
}
