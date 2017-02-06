# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      # GN: //tools/android/kerberos/SpnegoAuthenticator:spnego_authenticator_apk
      'target_name': 'spnego_authenticator_apk',
      'type': 'none',
      'variables': {
        'apk_name': 'SpnegoAuthenticator',
        'android_manifest_path': 'SpnegoAuthenticator/AndroidManifest.xml',
        'java_in_dir': '../../../tools/android/kerberos/SpnegoAuthenticator',
        'resource_dir': '../../../tools/android/kerberos/SpnegoAuthenticator/res',
      },
      'dependencies': [
        '../../../base/base.gyp:base_java'
        '../../../net/net.gyp:net_java'
      ],
      'includes': [ '../../../build/java_apk.gypi' ],
  }]
}
