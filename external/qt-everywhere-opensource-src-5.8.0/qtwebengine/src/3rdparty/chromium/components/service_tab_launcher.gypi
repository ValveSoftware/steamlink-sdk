# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN: //components/serivce_tab_launcher:service_tab_launcher
      'target_name': 'service_tab_launcher',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        'service_tab_launcher_jni_headers',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'service_tab_launcher/browser/android/service_tab_launcher.cc',
        'service_tab_launcher/browser/android/service_tab_launcher.h',
        'service_tab_launcher/component_jni_registrar.cc',
        'service_tab_launcher/component_jni_registrar.h',
      ],
    },
    {
      # GN: //components/serivce_tab_launcher:service_tab_launcher_java
      'target_name': 'service_tab_launcher_java',
      'type': 'none',
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_java',
      ],
      'variables': {
        'java_in_dir': 'service_tab_launcher/android/java',
      },
      'includes': [ '../build/java.gypi' ],
    },
    {
      # GN: //components/serivce_tab_launcher:service_tab_launcher_jni_headers
      'target_name': 'service_tab_launcher_jni_headers',
      'type': 'none',
      'sources': [
        'service_tab_launcher/android/java/src/org/chromium/components/service_tab_launcher/ServiceTabLauncher.java',
      ],
      'variables': {
        'jni_gen_package': 'service_tab_launcher',
      },
      'includes': [ '../build/jni_generator.gypi' ],
    },
  ],
}
