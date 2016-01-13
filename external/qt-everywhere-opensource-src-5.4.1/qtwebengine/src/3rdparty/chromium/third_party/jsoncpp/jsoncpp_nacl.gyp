# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/common_untrusted.gypi',
    'jsoncpp.gypi',
  ],
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'jsoncpp_nacl',
          'type': 'none',
          'variables': {
            'nacl_untrusted_build': 1,
            'nlib_target': 'libjsoncpp_nacl.a',
            'build_newlib': 1,
          },
          'gcc_compile_flags': [
            # Turn off optimizations based on strict aliasing
            # because of the workaround at
            # overrides/src/lib_json/json_value.cpp:38.
            '-fno-strict-aliasing',
          ],
          'dependencies': [
            '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
          ],
        },
      ],
    }],
  ],
}
