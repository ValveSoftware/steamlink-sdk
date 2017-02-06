# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# When making changes remember that this is shared with the internal .apk
# build rules.
{
  'type': 'none',
  'dependencies': [
    '<(DEPTH)/android_webview/android_webview.gyp:libwebviewchromium',
  ],
  'variables': {
    'native_lib_target': 'libwebviewchromium',
    'native_lib_version_name': '<(version_full)',
    'shared_resources': 1,
    'extensions_to_not_compress': '.lpak,.pak,.bin,.dat',
    'asset_location': '<(INTERMEDIATE_DIR)/assets/',
    'snapshot_copy_files': '<(snapshot_copy_files)',
    'jinja_inputs': ['<(android_manifest_template_path)'],
    'jinja_output': '<(INTERMEDIATE_DIR)/AndroidManifest.xml',
    'jinja_variables': [ '<@(android_manifest_template_vars)' ],
    'android_manifest_template_vars': [ ],
    'android_manifest_template_path': '<(DEPTH)/android_webview/apk/java/AndroidManifest.xml',
    'android_manifest_path': '<(jinja_output)',
    'proguard_enabled': 'true',
    'proguard_flags_paths': ['<(DEPTH)/android_webview/apk/java/proguard.flags'],
    # TODO: crbug.com/405035 Find a better solution for WebView .pak files.
    'additional_input_paths': [
      '<(asset_location)/webviewchromium.pak',
      '<(asset_location)/webview_licenses.notice',
      '<@(snapshot_additional_input_paths)',
    ],
    'includes': [
      '../../build/util/version.gypi',
      '../snapshot_copying.gypi',
    ],
    'conditions': [
      ['icu_use_data_file_flag==1', {
        'additional_input_paths': [
          '<(asset_location)/icudtl.dat',
        ],
      }],
    ],
  },
  'copies': [
    {
      'destination': '<(asset_location)',
      'files': [
        '<(webview_licenses_path)',
        '<(webview_chromium_pak_path)',
        '<@(snapshot_copy_files)',
      ],
      'conditions': [
        ['icu_use_data_file_flag==1', {
          'files': [
            '<(PRODUCT_DIR)/icudtl.dat',
          ],
        }],
      ],
    },
  ],
  'includes': [
    'system_webview_paks.gypi',
    '../../build/java_apk.gypi',
    '../../build/android/jinja_template.gypi',
  ],
}
