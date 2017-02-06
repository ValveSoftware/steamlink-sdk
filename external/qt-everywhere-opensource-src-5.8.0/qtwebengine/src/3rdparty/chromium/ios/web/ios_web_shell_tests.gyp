# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ios_web_shell_test',
      'type': 'loadable_module',
      'mac_xctest_bundle': 1,
      'variables': {
        'test_host': '<(_target_name)_host',
      },
      'dependencies': [
        '<(test_host)',
      ],
      'xcode_settings': {
        'WRAPPER_EXTENSION': 'xctest',
        'TEST_HOST': '${BUILT_PRODUCTS_DIR}/<(test_host).app/<(test_host)',
        'BUNDLE_LOADER': '$(TEST_HOST)',
        'CODE_SIGN_IDENTITY[sdk=iphoneos*]': 'iPhone Developer',
        'INFOPLIST_FILE': 'shell/test/Module-Info.plist',
        'OTHER_LDFLAGS': [
          '-bundle_loader <(test_host).app/<(test_host)',
        ],
      },
      'sources': [
        'shell/test/shell_test.mm',
      ],
      'link_settings': {
        'libraries': [
          'Foundation.framework',
          'XCTest.framework',
        ],
      },
    },
    {
      # Create a test host for earl grey tests, so Xcode 7.3 and above
      # doesn't contaminate the app structure.
      'target_name': 'ios_web_shell_test_host',
      'includes': [
        'ios_web_shell_exe.gypi',
      ],
      'link_settings': {
        'libraries': [
          'XCTest.framework',
        ],
      },
      'xcode_settings': {
        'INFOPLIST_FILE': 'shell/test/Host-Info.plist',
        'OTHER_LDFLAGS': [
          '-Xlinker', '-rpath', '-Xlinker', '@executable_path/Frameworks',
          '-Xlinker', '-rpath', '-Xlinker', '@loader_path/Frameworks'
        ]
      },
      'dependencies': [
        'ios_web_shell_earl_grey_test_support',
        '<(DEPTH)/ios/third_party/earl_grey/earl_grey.gyp:EarlGrey',
      ],
      'sources': [
        'shell/test/web_shell_navigation_egtest.mm',
      ],
      'actions': [{
        'action_name': 'copy_test_data',
        'variables': {
        'test_data_files': [
          '../../ios/web/shell/test/http_server_files',
        ],
        # Files are copied to .app/<test_data_prefix>/<test_data_files>.
        # Since the test_data_files are two levels up, the test_data_prefix
        # needs to be two levels deep so the files end up in the .app bundle
        # and not in some parent directory. In other words, this will resolve
        # to: .app/ios/web/../../ios/web/shell/test/http_server_files.
        'test_data_prefix': 'ios/web',
        },
        'includes': [ '../../build/copy_test_data_ios.gypi' ],
      }],

      'postbuilds': [
        {
          'postbuild_name': 'Copy OCHamcrest to TEST_HOST',
          'action': [
            'ditto',
            '${BUILT_PRODUCTS_DIR}/OCHamcrest.framework',
            '${BUILT_PRODUCTS_DIR}/<(_target_name).app/Frameworks/OCHamcrest.framework',
          ],
        },
        {
          'postbuild_name': 'Copy EarlGrey to TEST_HOST',
          'action': [
            'ditto',
            '${BUILT_PRODUCTS_DIR}/EarlGrey.framework',
            '${BUILT_PRODUCTS_DIR}/<(_target_name).app/Frameworks/EarlGrey.framework',
          ],
        },
      ],
    },
    {
      # TODO(crbug.com/606815): Refactor out code that is common across Chrome
      # and the web shell.
      'target_name': 'ios_web_shell_earl_grey_test_support',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/ios/third_party/earl_grey/earl_grey.gyp:EarlGrey',
        'ios_web.gyp:ios_web_earl_grey_test_support',
        '../testing/earl_grey/earl_grey_support.gyp:earl_grey_support',
      ],
      'sources': [
        'shell/test/app/navigation_test_util.h',
        'shell/test/app/navigation_test_util.mm',
        'shell/test/app/web_shell_test_util.h',
        'shell/test/app/web_shell_test_util.mm',
        'shell/test/app/web_view_interaction_test_util.h',
        'shell/test/app/web_view_interaction_test_util.mm',
        'shell/test/earl_grey/shell_matchers.h',
        'shell/test/earl_grey/shell_matchers.mm',
      ],
    },
  ],
}
