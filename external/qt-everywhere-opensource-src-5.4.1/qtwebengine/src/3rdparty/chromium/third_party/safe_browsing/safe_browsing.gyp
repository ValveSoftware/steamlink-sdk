# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'safe_browsing',
      'type': 'none',
      'sources': [
        'testing/external_test_pb2.py',
        'testing/safebrowsing_test_server.py',
        'testing/testing_input.dat',
      ],
      'export_dependent_settings': [
        '../protobuf/protobuf.gyp:py_proto',
      ],
      'dependencies': [
        '../protobuf/protobuf.gyp:py_proto',
      ],
     },
  ],
}
