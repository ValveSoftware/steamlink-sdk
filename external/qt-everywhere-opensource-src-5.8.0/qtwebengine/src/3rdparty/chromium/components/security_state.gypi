# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'security_state',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'security_state/security_state_model.cc',
        'security_state/security_state_model.h',
        'security_state/security_state_model_client.h',
        'security_state/switches.cc',
        'security_state/switches.h',
      ]
    },
  ],
  'conditions' : [
    ['OS=="android"', {
      'targets': [
        {
          # GN: //components/security_state:security_state_enums_java
          'target_name': 'security_state_enums_java',
          'type': 'none',
          'variables': {
            'source_file': 'security_state/security_state_model.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
      ],
     },
   ],
  ],
}
