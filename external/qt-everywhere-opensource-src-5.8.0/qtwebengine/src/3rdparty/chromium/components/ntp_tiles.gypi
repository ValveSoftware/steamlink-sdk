# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/ntp_tiles
      'target_name': 'ntp_tiles',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'history_core_browser',
        'pref_registry',
        'search_engines',
        'suggestions',
        'variations',
      ],
      'sources': [
        'ntp_tiles/constants.cc',
        'ntp_tiles/constants.h',
        'ntp_tiles/most_visited_sites.cc',
        'ntp_tiles/most_visited_sites.h',
        'ntp_tiles/popular_sites.cc',
        'ntp_tiles/popular_sites.h',
        'ntp_tiles/pref_names.h',
        'ntp_tiles/pref_names.cc',
        'ntp_tiles/switches.h',
        'ntp_tiles/switches.cc',
      ],
      'conditions': [
        ['OS != "ios"', {
          'dependencies': [
            'safe_json',
          ],
        }],
        ['OS == "android"', {
          'dependencies': [
            'ntp_tiles_jni_headers',
          ],
        }],
      ],
    },
  ],

  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          # GN: //components/ntp_tiles:ntp_tiles_enums_java
          'target_name': 'ntp_tiles_enums_java',
          'type': 'none',
          'variables': {
            'source_file': 'ntp_tiles/most_visited_sites.h',
          },
          'includes': [
            '../build/android/java_cpp_enum.gypi'
          ],
        },
        {
          # GN version: //components/ntp_tiles/android:ntp_tiles_java
          'target_name': 'ntp_tiles_java',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base',
          ],
          'variables': {
            'java_in_dir': 'ntp_tiles/android/java',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          # GN version: //components/ntp_tiles:jni
          'target_name': 'ntp_tiles_jni_headers',
          'type': 'none',
          'sources': [
            'ntp_tiles/android/java/src/org/chromium/components/ntptiles/MostVisitedSites.java',
          ],
          'variables': {
            'jni_gen_package': 'ntp_tiles',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
      ],
    }],
  ],
}
