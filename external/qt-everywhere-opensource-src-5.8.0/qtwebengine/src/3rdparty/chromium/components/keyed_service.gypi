# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/keyed_service/core:core
      'target_name': 'keyed_service_core',
      'type': '<(component)',
      'defines': [
        'KEYED_SERVICE_IMPLEMENTATION',
      ],
      'include_dirs': [
        '..',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
      'dependencies': [
        '../base/base.gyp:base',
        'prefs/prefs.gyp:prefs',
        'user_prefs',
      ],
      'sources': [
        'keyed_service/core/dependency_graph.cc',
        'keyed_service/core/dependency_graph.h',
        'keyed_service/core/dependency_manager.cc',
        'keyed_service/core/dependency_manager.h',
        'keyed_service/core/dependency_node.h',
        'keyed_service/core/keyed_service.cc',
        'keyed_service/core/keyed_service.h',
        'keyed_service/core/keyed_service_base_factory.cc',
        'keyed_service/core/keyed_service_base_factory.h',
        'keyed_service/core/keyed_service_export.h',
        'keyed_service/core/keyed_service_factory.cc',
        'keyed_service/core/keyed_service_factory.h',
        'keyed_service/core/keyed_service_shutdown_notifier.cc',
        'keyed_service/core/keyed_service_shutdown_notifier.h',
        'keyed_service/core/refcounted_keyed_service.cc',
        'keyed_service/core/refcounted_keyed_service.h',
        'keyed_service/core/refcounted_keyed_service_factory.cc',
        'keyed_service/core/refcounted_keyed_service_factory.h',
        'keyed_service/core/service_access_type.h',
      ],
    },
  ],
  'conditions': [
    ['OS != "ios"', {
      'targets': [
        {
          # GN version: //components/keyed_service/content:content
          'target_name': 'keyed_service_content',
          'type': '<(component)',
          'defines': [
            'KEYED_SERVICE_IMPLEMENTATION',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
          'dependencies': [
            '../base/base.gyp:base',
            '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
            '../content/content.gyp:content_common',
            'keyed_service_core',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'keyed_service/content/browser_context_dependency_manager.cc',
            'keyed_service/content/browser_context_dependency_manager.h',
            'keyed_service/content/browser_context_keyed_base_factory.cc',
            'keyed_service/content/browser_context_keyed_base_factory.h',
            'keyed_service/content/browser_context_keyed_service_factory.cc',
            'keyed_service/content/browser_context_keyed_service_factory.h',
            'keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.cc',
            'keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h',
            'keyed_service/content/refcounted_browser_context_keyed_service_factory.cc',
            'keyed_service/content/refcounted_browser_context_keyed_service_factory.h',
        ],
      }],
    }],
    ['OS == "ios"', {
      'targets': [
        {
          # GN version: //components/keyed_service/ios
          'target_name': 'keyed_service_ios',
          'type': '<(component)',
          'defines': [
            'KEYED_SERVICE_IMPLEMENTATION',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
            '../ios/web/ios_web.gyp:ios_web',
            'keyed_service_core',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'keyed_service/ios/browser_state_dependency_manager.cc',
            'keyed_service/ios/browser_state_dependency_manager.h',
            'keyed_service/ios/browser_state_keyed_service_factory.cc',
            'keyed_service/ios/browser_state_keyed_service_factory.h',
            'keyed_service/ios/refcounted_browser_state_keyed_service_factory.cc',
            'keyed_service/ios/refcounted_browser_state_keyed_service_factory.h',
        ],
      }],
    }],
  ],
}
