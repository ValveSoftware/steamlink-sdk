# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
     # GN version: //content/browser/speech/proto:proto
     'target_name': 'speech_proto',
     'type': 'static_library',
     'sources': [
        'google_streaming_api.proto',
      ],
     'variables': {
        'proto_in_dir': '.',
        'proto_out_dir': 'content/browser/speech/proto',
      },
     'includes': [ '../../../../build/protoc.gypi' ],
    },
  ],
}
