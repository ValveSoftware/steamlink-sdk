# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../build/common_untrusted.gypi',
  ],
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'nacl_io_untrusted',
          'type': 'none',
          'variables': {
            'nacl_untrusted_build': 1,
            'nlib_target': 'libnacl_io.a',
            'build_newlib': 1,
            'build_pnacl_newlib': 1,
          },
          'include_dirs': [
            '../native_client/src/untrusted/irt',
            'src/libraries',
            'src/libraries/nacl_io/include',
            'src/libraries/third_party/newlib-extras',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'src/libraries',
              'src/libraries/nacl_io/include',
              'src/libraries/third_party/newlib-extras',
            ],
          },
          'sources': [
            '>!@pymod_do_main(dsc_info -s -l src/libraries/nacl_io nacl_io)',
          ],
          'dependencies': [
            '../native_client/tools.gyp:prep_toolchain',
            '../ppapi/native_client/native_client.gyp:ppapi_lib',
          ],
        },
      ],
    }],
  ],
}
