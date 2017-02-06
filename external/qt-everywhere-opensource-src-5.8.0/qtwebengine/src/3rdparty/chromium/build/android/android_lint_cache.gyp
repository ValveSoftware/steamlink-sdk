# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # This target runs a functionally empty lint to create or update the
      # API versions cache if necessary. This prevents racy creation of the
      # cache while linting java targets in lint_action.gypi.
      'target_name': 'android_lint_cache',
      'type': 'none',
      'actions': [
        {
          'action_name': 'prepare_android_lint_cache',
          'message': 'Preparing Android lint cache',
          'variables': {
            'android_lint_cache_stamp': '<(PRODUCT_DIR)/android_lint_cache/android_lint_cache.stamp',
            'android_manifest_path': '<(DEPTH)/build/android/AndroidManifest.xml',
            'result_path': '<(PRODUCT_DIR)/android_lint_cache/result.xml',
            'platform_xml_path': '<(android_sdk_root)/platform-tools/api/api-versions.xml',
          },
          'inputs': [
            '<(DEPTH)/build/android/gyp/util/build_utils.py',
            '<(DEPTH)/build/android/gyp/lint.py',
            '<(android_manifest_path)',
            '<(platform_xml_path)',
          ],
          'outputs': [
            '<(android_lint_cache_stamp)',
            '<(result_path)',
          ],
          'action': [
            'python', '<(DEPTH)/build/android/gyp/lint.py',
            '--lint-path', '<(android_sdk_root)/tools/lint',
            '--cache-dir', '<(PRODUCT_DIR)/android_lint_cache',
            '--android-sdk-version=<(android_sdk_version)',
            '--platform-xml-path', '<(platform_xml_path)',
            '--manifest-path', '<(android_manifest_path)',
            '--product-dir', '<(PRODUCT_DIR)',
            '--result-path', '<(result_path)',
            '--stamp', '<(android_lint_cache_stamp)',
            '--create-cache',
            '--silent',
            '--enable'
          ],
        },
      ],
    },
  ],
}
