# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
    'package_name': 'chrome_shell_apk',
  },
  'includes': [
    'chrome_android_paks.gypi', # Included for the list of pak resources.
  ],
  'targets': [
    {
      'target_name': 'libchromeshell',
      'type': 'shared_library',
      'dependencies': [
        '../base/base.gyp:base',
        'chrome_android_core',
        'chrome.gyp:browser_ui',
        '../content/content.gyp:content_app_browser',
      ],
      'sources': [
        # This file must always be included in the shared_library step to ensure
        # JNI_OnLoad is exported.
        'app/android/chrome_jni_onload.cc',
        'android/shell/chrome_main_delegate_chrome_shell_android.cc',
        'android/shell/chrome_main_delegate_chrome_shell_android.h',
        "android/shell/chrome_shell_google_location_settings_helper.cc",
        "android/shell/chrome_shell_google_location_settings_helper.h",
      ],
      'include_dirs': [
        '../skia/config',
      ],
      'conditions': [
        [ 'order_profiling!=0', {
          'conditions': [
            [ 'OS=="android"', {
              'dependencies': [ '../tools/cygprofile/cygprofile.gyp:cygprofile', ],
            }],
          ],
        }],
        [ 'use_allocator!="none"', {
          'dependencies': [
            '../base/allocator/allocator.gyp:allocator', ],
        }],
        ['OS=="android"', {
          'ldflags': [
            # Some android targets still depend on --gc-sections to link.
            # TODO: remove --gc-sections for Debug builds (crbug.com/159847).
            '-Wl,--gc-sections',
          ],
        }],
      ],
    },
    {
      'target_name': 'chrome_shell_apk',
      'type': 'none',
      'dependencies': [
        'chrome_java',
        'chrome_shell_paks',
        'libchromeshell',
        '../media/media.gyp:media_java',
      ],
      'variables': {
        'apk_name': 'ChromeShell',
        'manifest_package_name': 'org.chromium.chrome.shell',
        'java_in_dir': 'android/shell/java',
        'resource_dir': 'android/shell/res',
        'asset_location': '<(PRODUCT_DIR)/../assets/<(package_name)',
        'native_lib_target': 'libchromeshell',
        'native_lib_version_name': '<(version_full)',
        'additional_input_paths': [
          '<@(chrome_android_pak_output_resources)',
        ],
        'conditions': [
          ['component != "shared_library" and target_arch != "arm64" and target_arch != "x64"', {
            # Only enable the chromium linker on regular builds, since the
            # component build crashes on Android 4.4. See b/11379966
            'use_chromium_linker': '1',
          }],
        ],
      },
      'includes': [ '../build/java_apk.gypi', ],
    },
    {
      # chrome_shell_apk creates a .jar as a side effect. Any java targets
      # that need that .jar in their classpath should depend on this target,
      # chrome_shell_apk_java. Dependents of chrome_shell_apk receive its
      # jar path in the variable 'apk_output_jar_path'.
      # This target should only be used by targets which instrument
      # chrome_shell_apk.
      'target_name': 'chrome_shell_apk_java',
      'type': 'none',
      'dependencies': [
        'chrome_shell_apk',
      ],
      'includes': [ '../build/apk_fake_jar.gypi' ],
    },
    {
      'target_name': 'chrome_android_core',
      'type': 'static_library',
      'dependencies': [
        'chrome.gyp:browser',
        'chrome.gyp:browser_ui',
        'chrome.gyp:plugin',
        'chrome.gyp:renderer',
        'chrome.gyp:utility',
        # TODO(kkimlabs): Move this to chrome.gyp:browser when the dependent
        #                 is upstreamed.
        '../components/components.gyp:enhanced_bookmarks',
        '../content/content.gyp:content',
        '../content/content.gyp:content_app_browser',
      ],
      'include_dirs': [
        '..',
        '<(android_ndk_include)',
      ],
      'sources': [
        'app/android/chrome_android_initializer.cc',
        'app/android/chrome_android_initializer.h',
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
      'target_name': 'chrome_shell_paks',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/chrome/chrome_resources.gyp:packed_resources',
        '<(DEPTH)/chrome/chrome_resources.gyp:packed_extra_resources',
      ],
      'copies': [
        {
          'destination': '<(chrome_android_pak_output_folder)',
          'files': [
            '<@(chrome_android_pak_input_resources)',
          ],
        }
      ],
    },
  ],
}
