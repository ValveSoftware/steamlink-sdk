# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'httpcomponents_client_javalib',
      'type' : 'none',
      'variables': {
        'input_jar_file': 'binary-distribution/lib/httpclient-4.2.2.jar',
        'pattern': 'org.apache.**',
      },
      'includes': [ '../jarjar/jarjar.gypi' ],
    },
    {
      'target_name': 'httpcomponents_mime_javalib',
      'type' : 'none',
      'variables': {
        'input_jar_file': 'binary-distribution/lib/httpmime-4.2.2.jar',
        'pattern': 'org.apache.**',
      },
      'includes': [ '../jarjar/jarjar.gypi' ],
    },
  ],
}
