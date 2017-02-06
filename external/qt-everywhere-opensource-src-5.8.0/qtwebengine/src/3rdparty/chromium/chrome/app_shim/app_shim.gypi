# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # This is the part of the shim process compiled into Chrome. It runs only
      # in the shim process, after the shim finds and loads the Chrome
      # Framework bundle.
      #
      # GN version: //chrome/app_shim
      'target_name': 'app_shim',
      'type': 'static_library',
      'dependencies': [
        # Since app_shim and browser depend on each other, we omit the
        # dependency on browser here.
        '../chrome/chrome_resources.gyp:chrome_strings',
        'app_mode_app_support',
      ],
      'sources': [
        'chrome_main_app_mode_mac.mm',
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '../..',
      ],
    },  # target app_shim
    {
      # This produces the template for app mode loader bundles. It's a template
      # in the sense that parts of it need to be "filled in" by Chrome before it
      # can be executed.
      'target_name': 'app_mode_app',
      'type': 'executable',
      'mac_bundle' : 1,
      'variables': {
        'enable_wexit_time_destructors': 1,
        'mac_real_dsym': 1,
      },
      'product_name': 'app_mode_loader',
      'dependencies': [
        'app_mode_app_support',
        'infoplist_strings_tool',
      ],
      'sources': [
        'app_mode_loader_mac.mm',
        'app_mode-Info.plist',
      ],
      'include_dirs': [
        '../..',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
          '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
        ],
      },
      'mac_bundle_resources!': [
        'app_shim/app_mode-Info.plist',
      ],
      'mac_bundle_resources/': [
        ['exclude', '.*'],
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'app_shim/app_mode-Info.plist',
        'APP_MODE_APP_BUNDLE_ID': '<(mac_bundle_id).app.@APP_MODE_SHORTCUT_ID@',
      },
      'postbuilds' : [
        {
          # Modify the Info.plist as needed.  The script explains why this
          # is needed.  This is also done in the chrome and chrome_dll
          # targets.  In this case, --breakpad=0, --keystone=0, and --scm=0
          # are used because Breakpad, Keystone, and SCM keys are
          # never placed into the app mode loader.
          'postbuild_name': 'Tweak Info.plist',
          'action': ['<(tweak_info_plist_path)',
                     '--plist=${TARGET_BUILD_DIR}/${INFOPLIST_PATH}',
                     '--breakpad=0',
                     '--keystone=0',
                     '--scm=0'],
        },
      ],
    },  # target app_mode_app
  ],  # targets
}
