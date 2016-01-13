# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'conditions': [
    ['OS == "android" and android_webview_build == 0', {
      'targets': [
        {
          'target_name': 'eyesfree_java',
          'type': 'none',
          'dependencies': [
            'eyesfree_aidl',
          ],
          'variables': {
            'java_in_dir': 'src/android/java',
          },
          'includes': [ '../../build/java.gypi' ],
        },
        {
          'target_name': 'eyesfree_aidl',
          'type': 'none',
          'variables': {
            'aidl_interface_file': '<(android_sdk)/framework.aidl',
            'aidl_import_include': 'src/android/java/src',
          },
          'sources': [
            'src/android/java/src/com/googlecode/eyesfree/braille/display/IBrailleService.aidl',
            'src/android/java/src/com/googlecode/eyesfree/braille/display/IBrailleServiceCallback.aidl',
            'src/android/java/src/com/googlecode/eyesfree/braille/selfbraille/ISelfBrailleService.aidl',
            'src/android/java/src/com/googlecode/eyesfree/braille/translate/ITranslatorService.aidl',
            'src/android/java/src/com/googlecode/eyesfree/braille/translate/ITranslatorServiceCallback.aidl',
          ],
          'includes': [ '../../build/java_aidl.gypi' ],
        },
      ],
    }],
  ],
}
