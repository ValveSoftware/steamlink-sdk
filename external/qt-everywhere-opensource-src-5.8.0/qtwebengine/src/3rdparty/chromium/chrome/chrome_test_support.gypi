# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Create a test support APK, and allow a test apk to depend on it, without the test
# apk incorporating the test support apk's Java code.
#
# Inputs:
#   test_support_apk_name - the name of the test support apk
#   test_support_apk_target - a GYP target name to use internally in this file.
#   test_support_apk_manifest_path - the full path of the manifest file for the test
#                                    support apk
#   test_support_dependencies - The dependencies of the test support APK. This should
#                               include all the APK's code.
#
# The corresponding test apk should depend on "require_<(test_support_apk_target)"
# It should not depend on "<(test_support_apk_target)" since, if it does, the test
# apk will incorporate the code of the test support apk.
{
  'conditions': [
    ['OS=="android"', {
      'variables' : {
        'test_support_apk_path': '<(PRODUCT_DIR)/apks/<(test_support_apk_name).apk'
      },
      'targets': [
        {
          'target_name': '<(test_support_apk_target)',
          'type': 'none',
          'dependencies': ['<@(test_support_apk_dependencies)',],
          'variables': {
            'apk_name': '<(test_support_apk_name)',
            'final_apk_path': '<(test_support_apk_path)',
            'java_in_dir': '<(DEPTH)/chrome/android/javatests',
            'java_in_dir_suffix': '/src_dummy',
            'android_manifest_path': '<(test_support_apk_manifest_path)',
         },
          'includes': [
            '../build/java_apk.gypi',
          ],
        },
        {
          # This emulates gn's datadeps fields, allowing other APKs to declare
          # that they require that this APK be built without including the
          # test_support's code.
          'target_name': 'require_<(test_support_apk_target)',
          'type': 'none',
          'actions': [
            {
              'action_name': 'require_<(test_support_apk_name)',
              'message': 'Making sure <(test_support_apk_path) has been built.',
              'variables': {
                'required_file': '<(PRODUCT_DIR)/test_support_apk/<(test_support_apk_name).apk.required',
              },
              'inputs': [
                '<(test_support_apk_path)',
              ],
              'outputs': [
                '<(required_file)',
              ],
              'action': [
                'python', '<(DEPTH)/build/android/gyp/touch.py', '<(required_file)',
              ],
            },
          ],
        },
      ],
    }],
  ],
}
