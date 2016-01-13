# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'version_py': '<(DEPTH)/build/util/version.py',
    'version_path': '<(DEPTH)/chrome/VERSION',
    'lastchange_path': '<(DEPTH)/build/util/LASTCHANGE',
    # 'branding_dir' is set in the 'conditions' section at the bottom.
    'msvs_use_common_release': 0,
    'msvs_use_common_linker_extras': 0,
  },
  'includes': [
    '../../build/win_precompile.gypi',
  ],
  'conditions': [
    # This target won't build in fastbuild, since there are no PDBs.
    ['OS=="win" and fastbuild==0', {
      'conditions': [
        ['chrome_multiple_dll==0', {
          'targets': [
            {
              'target_name': 'mini_installer_syzygy',
              'type': 'executable',
              'product_name': 'mini_installer',

              'variables': {
                'chrome_dll_project': [
                  '../chrome_syzygy.gyp:chrome_dll_syzygy',
                ],
                'chrome_dll_path': [
                  '<(PRODUCT_DIR)/syzygy/chrome.dll',
                ],
                'output_dir': '<(PRODUCT_DIR)/syzygy',
              },
              # Bulk of the build configuration comes from here.
              'includes': [ 'mini_installer.gypi', ],
            },
          ],
        }, {
          'targets': [
            {
              'target_name': 'mini_installer_syzygy',
              'type': 'executable',
              'product_name': 'mini_installer',

              'variables': {
                'chrome_dll_project': [
                  '../chrome_syzygy.gyp:chrome_dll_syzygy',
                  '../chrome_syzygy.gyp:chrome_child_dll_syzygy',
                ],
                'chrome_dll_path': [
                  '<(PRODUCT_DIR)/syzygy/chrome.dll',
                  '<(PRODUCT_DIR)/syzygy/chrome_child.dll',
                ],
                'output_dir': '<(PRODUCT_DIR)/syzygy',
              },
              # Bulk of the build configuration comes from here.
              'includes': [ 'mini_installer.gypi', ],
            },
          ],
        }],
      ],
    },{
      'targets': [],
    }],
    [ 'branding == "Chrome"', {
      'variables': {
         'branding_dir': '../app/theme/google_chrome',
      },
    }, {  # else branding!="Chrome"
      'variables': {
         'branding_dir': '../app/theme/chromium',
      },
    }],
  ],
}
