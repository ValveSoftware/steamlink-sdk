# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    # GN: //tools/android/push_apps_to_background:push_apps_to_background_apk
    {
      'target_name': 'push_apps_to_background_apk',
      'type': 'none',
      'variables': {
        'apk_name': 'PushAppsToBackground',
        'java_in_dir': '.',
        'resource_dir': 'res',
        'android_manifest_path': 'AndroidManifest.xml',
      },
      'includes': [
        '../../../build/java_apk.gypi',
      ],
    },
  ],
}
