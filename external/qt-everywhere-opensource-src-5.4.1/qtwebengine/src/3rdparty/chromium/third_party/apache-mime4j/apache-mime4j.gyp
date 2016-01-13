# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'apache_mime4j_javalib',
      'type' : 'none',
      'variables': {
        'input_jar_file': 'binary-distribution/lib/apache-mime4j-core-0.7.2.jar',
        'pattern': 'org.apache.**',
      },
      'includes': [ '../jarjar/jarjar.gypi' ],
    },
  ],
}
