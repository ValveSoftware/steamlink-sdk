# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN: //third_party/cardboard-java:cardboard_java
      'target_name': 'cardboard_jar',
      'type': 'none',
      'dependencies': [
        '../android_protobuf/android_protobuf.gyp:protobuf_nano_javalib',
      ],
      'variables': {
        'jar_path': 'src/CardboardSample/libs/cardboard.jar',
      },
      'includes': [
        '../../build/java_prebuilt.gypi',
      ]
    },
  ],
}

