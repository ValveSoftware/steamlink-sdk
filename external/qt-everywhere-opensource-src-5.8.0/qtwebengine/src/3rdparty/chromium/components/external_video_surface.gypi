# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1
  },
  'targets': [
    {
      'target_name': 'external_video_surface',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        'external_video_surface_jni_headers',
      ],
      'sources': [
        'external_video_surface/browser/android/external_video_surface_container_impl.cc',
        'external_video_surface/browser/android/external_video_surface_container_impl.h',
        'external_video_surface/component_jni_registrar.cc',
        'external_video_surface/component_jni_registrar.h',
      ],
    },
    {
      'target_name': 'external_video_surface_java',
      'type': 'none',
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_java',
      ],
      'variables': {
        'java_in_dir': 'external_video_surface/android/java',
      },
      'includes': [ '../build/java.gypi' ],
    },
    {
      'target_name': 'external_video_surface_jni_headers',
      'type': 'none',
      'sources': [
        'external_video_surface/android/java/src/org/chromium/components/external_video_surface/ExternalVideoSurfaceContainer.java',
      ],
      'variables': {
        'jni_gen_package': 'external_video_surface',
      },
      'includes': [ '../build/jni_generator.gypi' ],
    },
  ],
}
