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
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'validate_installation',
          'type': 'executable',
          'dependencies': [
            '<(DEPTH)/base/base.gyp:base',
            '<(DEPTH)/chrome/chrome.gyp:installer_util',
            '<(DEPTH)/chrome/chrome.gyp:installer_util_strings',
            '<(DEPTH)/chrome/common_constants.gyp:common_constants',
          ],
          'include_dirs': [
            '<(DEPTH)',
          ],
          'sources': [
            'tools/validate_installation_main.cc',
            'tools/validate_installation.rc',
            'tools/validate_installation_resource.h',
          ],
        },
      ],
    }],
    [ 'branding == "Chrome"', {
      'variables': {
         'branding_dir': '<(DEPTH)/chrome/app/theme/google_chrome',
      },
    }, { # else branding!="Chrome"
      'variables': {
         'branding_dir': '<(DEPTH)/chrome/app/theme/chromium',
      },
    }],
  ],
}
