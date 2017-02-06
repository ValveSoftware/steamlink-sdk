{
  'targets': [
    {
      # GN version: //content/browser/cache_storage:cache_storage_proto
      'target_name': 'cache_storage_proto',
      'type': 'static_library',
      'sources': [
        'cache_storage.proto',
      ],
      'variables': {
        'proto_in_dir': '.',
        'proto_out_dir': 'content/browser/cache_storage',
      },
      'includes': [ '../../../build/protoc.gypi' ]
    },
  ],
}
