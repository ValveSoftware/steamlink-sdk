# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    { # This target references prebuilt Netty Jar file.
      'target_name': 'netty_all',
      'type' : 'none',
      'variables': {
        'jar_path': 'src/jar/all-in-one/netty-all-4.1.0.CR1-20160111.120759-50.jar',
        'exclude_from_apk': 1,
      },
      'includes': ['../../build/java_prebuilt.gypi'],
    },
  ],
}