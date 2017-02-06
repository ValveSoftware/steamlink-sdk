{
  'targets': [
    {
      # GN version: //content/browser/background_sync:background_sync_proto
      'target_name': 'background_sync_proto',
      'type': 'static_library',
      'sources': [
        'background_sync.proto',
      ],
      'variables': {
        'proto_in_dir': '.',
        'proto_out_dir': 'content/browser/background_sync',
      },
      'includes': [ '../../../build/protoc.gypi' ]
    },
  ],
}
