# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //extensions/common/api:mojom
      'target_name': 'extensions_api_mojom',
      # The type of this target must be none. This is so that resources can
      # depend upon this target for generating the js bindings files. Any
      # generated cpp files must be listed explicitly in chrome_api.
      'type': 'none',
      'includes': [
        '../../../mojo/mojom_bindings_generator.gypi',
      ],
      'sources': [
        'mime_handler.mojom',
      ],
    },
    {
      # GN version: //extensions/common/api
      'target_name': 'extensions_api',
      'type': 'static_library',
      # TODO(jschuh): http://crbug.com/167187 size_t -> int
      'msvs_disabled_warnings': [ 4267 ],
      'includes': [
        '../../../build/json_schema_bundle_compile.gypi',
        '../../../build/json_schema_compile.gypi',
        'schemas.gypi',
      ],
      'dependencies': [
        'extensions_api_mojom',
        '../../../mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/extensions/common/api/mime_handler.mojom.cc',
        '<(SHARED_INTERMEDIATE_DIR)/extensions/common/api/mime_handler.mojom.h',
      ],
    },
    {
      # Protobuf compiler / generator for chrome.cast.channel-related protocol buffers.
      # GN version: //extensions/browser/api/cast_channel:cast_channel_proto
      'target_name': 'cast_channel_proto',
      'type': 'static_library',
      'sources': [
          'cast_channel/authority_keys.proto',
          'cast_channel/cast_channel.proto',
          'cast_channel/logging.proto',
      ],
      'variables': {
          'proto_in_dir': 'cast_channel',
          'proto_out_dir': 'extensions/common/api/cast_channel',
      },
      'includes': [ '../../../build/protoc.gypi' ]
    },
  ],
}
