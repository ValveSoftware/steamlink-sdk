# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into an action to provide a rule to
# run lint on java/class files.

{
  'action_name': 'lint_<(_target_name)',
  'message': 'Linting <(_target_name)',
  'variables': {
    'conditions': [
      ['chromium_code != 0 and android_lint != 0 and never_lint == 0', {
        'additional_args': ['--enable'],
      }, {
        'additional_args': [],
      }]
    ],
    'android_lint_cache_stamp': '<(PRODUCT_DIR)/android_lint_cache/android_lint_cache.stamp',
    'android_manifest_path%': '<(DEPTH)/build/android/AndroidManifest.xml',
    'resource_dir%': '<(DEPTH)/build/android/ant/empty/res',
    'suppressions_file%': '<(DEPTH)/build/android/lint/suppressions.xml',
    'platform_xml_path': '<(android_sdk_root)/platform-tools/api/api-versions.xml',
  },
  'inputs': [
    '<(DEPTH)/build/android/gyp/util/build_utils.py',
    '<(DEPTH)/build/android/gyp/lint.py',
    '<(android_lint_cache_stamp)',
    '<(android_manifest_path)',
    '<(lint_jar_path)',
    '<(suppressions_file)',
    '<(platform_xml_path)',
  ],
  'action': [
    'python', '<(DEPTH)/build/android/gyp/lint.py',
    '--lint-path=<(android_sdk_root)/tools/lint',
    '--config-path=<(suppressions_file)',
    '--processed-config-path=<(config_path)',
    '--cache-dir', '<(PRODUCT_DIR)/android_lint_cache',
    '--platform-xml-path', '<(platform_xml_path)',
    '--manifest-path=<(android_manifest_path)',
    '--result-path=<(result_path)',
    '--resource-dir=<(resource_dir)',
    '--product-dir=<(PRODUCT_DIR)',
    '--src-dirs=>(src_dirs)',
    '--jar-path=<(lint_jar_path)',
    '--can-fail-build',
    '--stamp=<(stamp_path)',
    '<@(additional_args)',
  ],
}
