# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/safe_json
      'target_name': 'safe_json',
      'type': 'static_library',
      'dependencies': [
        'safe_json_mojo_bindings',
        '../base/base.gyp:base',
        '../components/components_strings.gyp:components_strings',
        '../content/content.gyp:content_browser',
        '../content/content.gyp:content_common',
        '../ui/base/ui_base.gyp:ui_base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'safe_json/android/component_jni_registrar.cc',
        'safe_json/android/component_jni_registrar.h',
        'safe_json/json_sanitizer.cc',
        'safe_json/json_sanitizer.h',
        'safe_json/json_sanitizer_android.cc',
        'safe_json/safe_json_parser.cc',
        'safe_json/safe_json_parser.h',
        'safe_json/safe_json_parser_android.cc',
        'safe_json/safe_json_parser_android.h',
        'safe_json/safe_json_parser_impl.cc',
        'safe_json/safe_json_parser_impl.h',
      ],
      'conditions': [
        ['OS == "android"', {
          'dependencies': [
            'safe_json_jni_headers',
          ],
          'sources!': [
            'safe_json/json_sanitizer.cc',
            'safe_json/safe_json_parser_impl.cc',
            'safe_json/safe_json_parser_impl.h',
          ],
        }],
      ],
    },
    {
      'target_name': 'safe_json_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        ':safe_json',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'safe_json/testing_json_parser.cc',
        'safe_json/testing_json_parser.h',
      ],
    },
    {
      # GN version: //components/safe_json/public/interfaces
      'target_name': 'safe_json_mojo_bindings',
      'type': 'static_library',
      'variables': {
        'mojom_typemaps': [
          'safe_json/public/interfaces/safe_json.typemap',
        ],
      },
      'sources': [
        'safe_json/public/interfaces/safe_json.mojom',
      ],
      'includes': [ '../mojo/mojom_bindings_generator.gypi'],
    },
    {
      'target_name': 'safe_json_parser_mojo',
      'type': 'static_library',
      'dependencies': [
        'safe_json_mojo_bindings',
        '../base/base.gyp:base',
        '../content/content.gyp:content_common',
        '../content/content.gyp:content_utility',
        '../ipc/ipc.gyp:ipc',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'safe_json/utility/safe_json_parser_mojo_impl.cc',
        'safe_json/utility/safe_json_parser_mojo_impl.h',
      ],
    },
  ],
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          # GN version: //components/safe_json/android:safe_json_java
          'target_name': 'safe_json_java',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base',
          ],
          'variables': {
            'java_in_dir': 'safe_json/android/java',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          # GN version: //components/safe_json:jni
          'target_name': 'safe_json_jni_headers',
          'type': 'none',
          'sources': [
            'safe_json/android/java/src/org/chromium/components/safejson/JsonSanitizer.java',
          ],
          'variables': {
            'jni_gen_package': 'safe_json',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
      ],
    }],
  ]
}
