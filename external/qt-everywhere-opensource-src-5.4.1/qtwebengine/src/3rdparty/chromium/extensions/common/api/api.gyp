# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'extensions_api',
      'type': 'static_library',
      'sources': [
        '<@(schema_files)',
      ],
      # TODO(jschuh): http://crbug.com/167187 size_t -> int
      'msvs_disabled_warnings': [ 4267 ],
      'includes': [
        '../../../build/json_schema_bundle_compile.gypi',
        '../../../build/json_schema_compile.gypi',
      ],
      'variables': {
        'chromium_code': 1,
        'non_compiled_schema_files': [
        ],
        'conditions': [
          ['enable_extensions==1', {
            'schema_files': [
              'dns.idl',
              'extensions_manifest_types.json',
              'runtime.json',
              'socket.idl',
              'sockets_tcp.idl',
              'sockets_tcp_server.idl',
              'sockets_udp.idl',
              'storage.json',
              'test.json',
              'usb.idl',
            ],
          }, {
            # TODO: Eliminate these on Android. See crbug.com/305852.
            'schema_files': [
              'extensions_manifest_types.json',
              'runtime.json',
            ],
          }],
        ],
        'cc_dir': 'extensions/common/api',
        'root_namespace': 'extensions::core_api',
        'impl_dir': 'extensions/browser/api',
      },
      'dependencies': [
        '<(DEPTH)/skia/skia.gyp:skia',
      ],
    },
  ],
}
