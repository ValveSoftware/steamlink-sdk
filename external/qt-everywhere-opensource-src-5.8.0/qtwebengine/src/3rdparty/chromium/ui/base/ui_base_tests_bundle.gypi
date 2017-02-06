# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file contains resources for the ui_base_unittests test bundle.
# See chrome_dll_bundle.gypi for a description of the techniques here.
{
  'product_name': 'ui_unittests Framework',
  'variables': {
    # There is no executable in the mock framework, and so nothing to strip.
    'mac_strip': 0,
  },
  'mac_bundle': 1,
  'xcode_settings': {
    'CHROMIUM_BUNDLE_ID': 'com.google.ChromiumUITests',
    'DYLIB_COMPATIBILITY_VERSION': '1.0.0',
    'DYLIB_CURRENT_VERSION': '1.0.0',
    'DYLIB_INSTALL_NAME_BASE': '@executable_path/../Versions/1.0.0.0',
    'LD_DYLIB_INSTALL_NAME':
        '$(DYLIB_INSTALL_NAME_BASE:standardizepath)/$(WRAPPER_NAME)/$(PRODUCT_NAME)',
    'INFOPLIST_FILE': 'test/framework-Info.plist',
  },
  'mac_bundle_resources': [
    'test/framework-Info.plist',
    '<(PRODUCT_DIR)/ui_test.pak',

    # Just include the English-US locale made by ui_resources.gyp:ui_test_pak.
    '<(PRODUCT_DIR)/ui/en.lproj/locale.pak',
  ],
  'mac_bundle_resources!': [
    'test/framework-Info.plist',
  ],
  'postbuilds': [
    {
      'postbuild_name': 'Symlink Resources',
      'action': [
        'ln',
        '-fns',
        'Versions/A/Resources',
        '${BUILT_PRODUCTS_DIR}/${WRAPPER_NAME}/Resources'
      ],
    },
    {
      # Resource bundle pak names are hardcoded. This allows ui_test.pak to be
      # found while running the ResourceBundle tests.
      'postbuild_name': 'Symlink chrome_100_percent for test',
      'action': [
        'ln',
        '-fns',
        'ui_test.pak',
        '${BUILT_PRODUCTS_DIR}/${WRAPPER_NAME}/Versions/A/Resources/chrome_100_percent.pak'
      ],
    },
  ],
}
