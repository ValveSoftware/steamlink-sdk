# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      # GN: //tools/android/audio_focus_grabber:audio_focus_grabber_apk
      'target_name': 'audio_focus_grabber_apk',
      'type': 'none',
      'variables': {
        'apk_name': 'AudioFocusGrabber',
        'android_manifest_path': 'java/AndroidManifest.xml',
        'java_in_dir': 'java',
        'resource_dir': 'java/res',
      },
      'dependencies': [
        '../../../base/base.gyp:base_java',
        '../../../third_party/android_tools/android_tools.gyp:android_support_v13_javalib'
      ],
      'includes': [ '../../../build/java_apk.gypi' ],
  }]
}
