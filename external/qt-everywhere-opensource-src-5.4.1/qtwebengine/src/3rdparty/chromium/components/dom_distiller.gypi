# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['android_webview_build == 0', {
      'targets': [
        {
          'target_name': 'dom_distiller_webui',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../content/content.gyp:content_browser',
            '../skia/skia.gyp:skia',
            '../sync/sync.gyp:sync',
            'components_resources.gyp:components_resources',
            'components_strings.gyp:components_strings',
            'distilled_page_proto',
            'dom_distiller_core',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'dom_distiller/webui/dom_distiller_handler.cc',
            'dom_distiller/webui/dom_distiller_handler.h',
            'dom_distiller/webui/dom_distiller_ui.cc',
            'dom_distiller/webui/dom_distiller_ui.h',
          ],
        },
        {
          'target_name': 'dom_distiller_core',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../skia/skia.gyp:skia',
            '../sync/sync.gyp:sync',
            '../third_party/dom_distiller_js/dom_distiller_js.gyp:dom_distiller_js_proto',
            'components.gyp:leveldb_proto',
            'components_resources.gyp:components_resources',
            'components_strings.gyp:components_strings',
            'distilled_page_proto',
          ],
          'include_dirs': [
            '..',
          ],
          'export_dependent_settings': [
            'distilled_page_proto',
            '../third_party/dom_distiller_js/dom_distiller_js.gyp:dom_distiller_js_proto',
          ],
          'sources': [
            'dom_distiller/android/component_jni_registrar.cc',
            'dom_distiller/android/component_jni_registrar.h',
            'dom_distiller/core/article_distillation_update.cc',
            'dom_distiller/core/article_distillation_update.h',
            'dom_distiller/core/article_entry.cc',
            'dom_distiller/core/article_entry.h',
            'dom_distiller/core/distilled_content_store.cc',
            'dom_distiller/core/distilled_content_store.h',
            'dom_distiller/core/distiller.cc',
            'dom_distiller/core/distiller.h',
            'dom_distiller/core/distiller_page.cc',
            'dom_distiller/core/distiller_page.h',
            'dom_distiller/core/distiller_url_fetcher.cc',
            'dom_distiller/core/distiller_url_fetcher.h',
            'dom_distiller/core/dom_distiller_constants.cc',
            'dom_distiller/core/dom_distiller_constants.h',
            'dom_distiller/core/dom_distiller_model.cc',
            'dom_distiller/core/dom_distiller_model.h',
            'dom_distiller/core/dom_distiller_observer.h',
            'dom_distiller/core/dom_distiller_service.cc',
            'dom_distiller/core/dom_distiller_service.h',
            'dom_distiller/core/dom_distiller_store.cc',
            'dom_distiller/core/dom_distiller_store.h',
            'dom_distiller/core/feedback_reporter.cc',
            'dom_distiller/core/feedback_reporter.h',
            'dom_distiller/core/task_tracker.cc',
            'dom_distiller/core/task_tracker.h',
            'dom_distiller/core/url_constants.cc',
            'dom_distiller/core/url_constants.h',
            'dom_distiller/core/url_utils_android.cc',
            'dom_distiller/core/url_utils_android.h',
            'dom_distiller/core/url_utils.cc',
            'dom_distiller/core/url_utils.h',
            'dom_distiller/core/viewer.cc',
            'dom_distiller/core/viewer.h',
          ],
          'conditions': [
            ['OS == "android"', {
              'dependencies': [
                'dom_distiller_core_jni_headers',
              ],
            }],
          ],
        },
        {
          'target_name': 'dom_distiller_test_support',
          'type': 'static_library',
          'dependencies': [
            'dom_distiller_core',
            'components.gyp:leveldb_proto_test_support',
            '../sync/sync.gyp:sync',
            '../testing/gmock.gyp:gmock',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'dom_distiller/core/dom_distiller_test_util.cc',
            'dom_distiller/core/dom_distiller_test_util.h',
            'dom_distiller/core/fake_distiller.cc',
            'dom_distiller/core/fake_distiller.h',
            'dom_distiller/core/fake_distiller_page.cc',
            'dom_distiller/core/fake_distiller_page.h',
          ],
        },
        {
          'target_name': 'distilled_page_proto',
          'type': 'static_library',
          'sources': [
            'dom_distiller/core/proto/distilled_article.proto',
            'dom_distiller/core/proto/distilled_page.proto',
          ],
          'variables': {
            'proto_in_dir': 'dom_distiller/core/proto',
            'proto_out_dir': 'components/dom_distiller/core/proto',
          },
          'includes': [ '../build/protoc.gypi' ]
        },
      ],
      'conditions': [
        ['OS != "ios"', {
          'targets': [
            {
              'target_name': 'dom_distiller_content',
              'type': 'static_library',
              'dependencies': [
                '../content/content.gyp:content_browser',
                '../net/net.gyp:net',
                '../skia/skia.gyp:skia',
                '../sync/sync.gyp:sync',
                'components_resources.gyp:components_resources',
                'components_strings.gyp:components_strings',
                'dom_distiller_core',
              ],
              'include_dirs': [
                '..',
              ],
              'sources': [
                'dom_distiller/content/distiller_page_web_contents.cc',
                'dom_distiller/content/distiller_page_web_contents.h',
                'dom_distiller/content/dom_distiller_viewer_source.cc',
                'dom_distiller/content/dom_distiller_viewer_source.h',
                'dom_distiller/content/web_contents_main_frame_observer.cc',
                'dom_distiller/content/web_contents_main_frame_observer.h',
              ],
            },
          ],
        }],
        ['OS=="android"', {
          'targets': [
            {
              'target_name': 'dom_distiller_core_java',
              'type': 'none',
              'dependencies': [
                '../base/base.gyp:base',
              ],
              'variables': {
                'java_in_dir': 'dom_distiller/android/java',
              },
              'includes': [ '../build/java.gypi' ],
            },
            {
              'target_name': 'dom_distiller_core_jni_headers',
              'type': 'none',
              'sources': [
                'dom_distiller/android/java/src/org/chromium/components/dom_distiller/core/DomDistillerUrlUtils.java',
              ],
              'variables': {
                'jni_gen_package': 'dom_distiller_core',
              },
              'includes': [ '../build/jni_generator.gypi' ],
            },
          ],
        }],
      ],
    }],
  ],
}
