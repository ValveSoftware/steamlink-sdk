# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'memconsumer',
      'type': 'none',
      'dependencies': [
        'memconsumer_apk',
      ],
    },
    {
      'target_name': 'memconsumer_apk',
      'type': 'none',
      'variables': {
        'apk_name': 'MemConsumer',
        'package_name': 'memconsumer',
        'java_in_dir': 'java',
        'resource_dir': 'java/res',
        'native_lib_target': 'libmemconsumer',
      },
      'dependencies': [
        'libmemconsumer',
      ],
      'includes': [ '../../../build/java_apk.gypi' ],
    },
    {
      'target_name': 'libmemconsumer',
      'type': 'shared_library',
      'sources': [
        'memconsumer_hook.cc',
      ],
      'libraries': [
        '-llog',
      ],
    },
  ],
}
