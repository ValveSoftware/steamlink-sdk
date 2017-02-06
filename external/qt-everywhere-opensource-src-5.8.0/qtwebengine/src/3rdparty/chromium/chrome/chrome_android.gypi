# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN: //chrome:chrome_android_core
      'target_name': 'chrome_android_core',
      'type': 'static_library',
      'dependencies': [
        'chrome.gyp:browser',
        'chrome.gyp:browser_ui',
        'chrome.gyp:child',
        'chrome.gyp:gpu',
        'chrome_features.gyp:chrome_common_features',
        'chrome.gyp:renderer',
        'chrome.gyp:utility',
        '../components/components.gyp:safe_browsing_db_mobile',
        '../content/content.gyp:content',
        '../content/content.gyp:content_app_both',
      ],
      'include_dirs': [
        '..',
        '<(android_ndk_include)',
      ],
      'sources': [
        'app/android/chrome_android_initializer.cc',
        'app/android/chrome_android_initializer.h',
        'app/android/chrome_jni_onload.cc',
        'app/android/chrome_jni_onload.h',
        'app/android/chrome_main_delegate_android.cc',
        'app/android/chrome_main_delegate_android.h',
        'app/chrome_main_delegate.cc',
        'app/chrome_main_delegate.h',
      ],
      'link_settings': {
        'libraries': [
          '-landroid',
          '-ljnigraphics',
        ],
      },
    },
    {
      # GYP: //chrome/android:chrome_version_java
      'target_name': 'chrome_version_java',
      'type': 'none',
      'variables': {
        'template_input_path': 'android/java/ChromeVersionConstants.java.version',
        'version_path': 'VERSION',
        'version_py_path': '<(DEPTH)/build/util/version.py',
        'output_path': '<(SHARED_INTERMEDIATE_DIR)/templates/<(_target_name)/org/chromium/chrome/browser/ChromeVersionConstants.java',

        'conditions': [
          ['branding == "Chrome"', {
            'branding_path': 'app/theme/google_chrome/BRANDING',
          }, {
            'branding_path': 'app/theme/chromium/BRANDING',
          }],
        ],
      },
      'direct_dependent_settings': {
        'variables': {
          # Ensure that the output directory is used in the class path
          # when building targets that depend on this one.
          'generated_src_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)/templates/<(_target_name)',
          ],
          # Ensure dependents are rebuilt when the generated Java file changes.
          'additional_input_paths': [
            '<(output_path)',
          ],
        },
      },
      'actions': [
        {
          'action_name': 'chrome_version_java_template',
          'inputs': [
            '<(template_input_path)',
            '<(version_path)',
            '<(branding_path)',
            '<(version_py_path)',
          ],
          'outputs': [
            '<(output_path)',
          ],
          'action': [
            'python',
            '<(version_py_path)',
            '-f', '<(version_path)',
            '-f', '<(branding_path)',
            '-e', 'CHANNEL=str.upper("<(android_channel)")',
            '<(template_input_path)',
            '<(output_path)',
          ],
        },
      ],
    },
  ],
}
