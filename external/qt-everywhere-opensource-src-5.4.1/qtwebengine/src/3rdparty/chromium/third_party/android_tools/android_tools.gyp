# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'android_java',
      'type' : 'none',
      'variables': {
        'jar_path': '<(android_sdk)/android.jar',
        'exclude_from_apk': 1,
      },
      'includes': ['../../build/java_prebuilt.gypi'],
    },
    {
      'target_name': 'android_gcm',
      'type' : 'none',
      'variables': {
        'jar_path': '<(android_sdk_root)/extras/google/gcm/gcm-client/dist/gcm.jar',
      },
      'includes': ['../../build/java_prebuilt.gypi'],
    },
    {
      'target_name': 'uiautomator_jar',
      'type': 'none',
      'variables': {
        'jar_path': '<(android_sdk)/uiautomator.jar',
      },
      'includes': ['../../build/java_prebuilt.gypi'],
    },
    {
      # This jar contains the Android support v13 library from the revision 18
      # of the Android Support library.
      'target_name': 'android_support_v13_javalib',
      'type' : 'none',
      'variables': {
        'jar_path': '<(android_sdk_root)/extras/android/support/v13/android-support-v13.jar',
      },
      'includes': ['../../build/java_prebuilt.gypi'],
    },
    {
      # This jar contains the Android support v7 appcompat library from the
      # revision 18 of the Android Support library. This library doesn't
      # contain the resources needed for the library to work.
      # TODO(avayvod): Add the resources directly once crbug.com/274697 is
      # fixed.
      'target_name': 'android_support_v7_appcompat_javalib_no_res',
      'type' : 'none',
      'variables': {
        'jar_path': '<(android_sdk_root)/extras/android/support/v7/appcompat/libs/android-support-v7-appcompat.jar',
      },
      'includes': ['../../build/java_prebuilt.gypi'],
    },
    {
      # This jar contains the Android support v7 mediarouter library from the
      # revision 18 of the Android Support library. This library doesn't
      # contain the resources needed for the library to work.
      # TODO(avayvod): Add the resources directly once crbug.com/274697 is
      # fixed.
      'target_name': 'android_support_v7_mediarouter_javalib_no_res',
      'type' : 'none',
      'variables': {
        'jar_path': '<(android_sdk_root)/extras/android/support/v7/mediarouter/libs/android-support-v7-mediarouter.jar',
      },
      'includes': ['../../build/java_prebuilt.gypi'],
    },
    {
      # This target contains the Android support v7 appcompat library with the
      # resources needed.
      'target_name': 'android_support_v7_appcompat_javalib',
      'type': 'none',
      'variables': {
        'java_in_dir': '<(android_sdk_root)/extras/android/support/v7/appcompat',
        'R_package': ['android.support.v7.appcompat'],
        'R_package_relpath': ['android/support/v7/appcompat'],
        'has_java_resources': 1,
        'res_v14_verify_only': 1,
      },
      'dependencies': [
        'android_support_v7_appcompat_javalib_no_res',
      ],
      'includes': [ '../../build/java.gypi' ]
    },
    {
      # This target contains the Android support v7 mediarouter library with the
      # resources needed.
      'target_name': 'android_support_v7_mediarouter_javalib',
      'type': 'none',
      'variables': {
        'java_in_dir': '<(android_sdk_root)/extras/android/support/v7/mediarouter',
        'R_package': ['android.support.v7.mediarouter'],
        'R_package_relpath': ['android/support/v7/mediarouter'],
        'has_java_resources': 1,
        'res_v14_verify_only': 1,
      },
      'dependencies': [
        'android_support_v7_mediarouter_javalib_no_res',
        'android_support_v7_appcompat_javalib',
      ],
      'includes': [ '../../build/java.gypi' ]
    },
  ],
}
