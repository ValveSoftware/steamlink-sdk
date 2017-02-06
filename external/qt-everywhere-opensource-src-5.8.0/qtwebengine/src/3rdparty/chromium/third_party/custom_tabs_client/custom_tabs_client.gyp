# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'custom_tabs_client_example_apk',
      'type': 'none',
      'variables': {
        'java_in_dir': 'src/Application/src/main/',
        'java_in_dir_suffix': 'java/',
        'resource_dir': '<(java_in_dir)/res',
        'apk_name': 'CustomTabsClientExample',
        'run_findbugs': 0,
      },
      'dependencies': [
        'custom_tabs_support_lib',
        'custom_tabs_client_shared_lib',
      ],
      'includes': [ '../../build/java_apk.gypi' ],
    },
    {
      'target_name': 'custom_tabs_client_shared_lib',
      'type': 'none',
      'variables': {
        'java_in_dir': 'src/shared/src/main/java/org/chromium/customtabsclient/shared/',
        'java_in_dir_suffix': '',
      },
      'dependencies': [
        'custom_tabs_support_lib',
      ],
      'includes': [ '../../build/java.gypi' ],
    },
    {
      'target_name': 'custom_tabs_support_lib',
      'type': 'none',
      'variables': {
        'java_in_dir': 'src/customtabs/src/',
        'java_in_dir_suffix': '',
      },
      'dependencies': [
        'chrome_custom_tabs_service_aidl',
        '../android_tools/android_tools.gyp:android_support_v13_javalib',
      ],
      'includes': [ '../../build/java.gypi' ],
    },
    {
      'target_name': "chrome_custom_tabs_service_aidl",
      'type': 'none',
      'variables': {
        'java_in_dir': 'src/customtabs/src/android/support/customtabs/',
        'java_in_dir_suffix': '',
        # The filename should not end with ".aidl", as it upsets gradle.
        # 'aidl_interface_file': '<(java_in_dir)/common.aidl.txt',
        'aidl_interface_file': 'common.aidl',
      },
      'sources': [
        '<(java_in_dir)/ICustomTabsCallback.aidl',
        '<(java_in_dir)/ICustomTabsService.aidl',
      ],
      'includes': [ '../../build/java_aidl.gypi' ],
    },
  ],
}
