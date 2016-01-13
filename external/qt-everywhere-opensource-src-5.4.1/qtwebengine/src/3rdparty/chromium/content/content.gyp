# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,  # Use higher warning level.
    'chromium_enable_vtune_jit_for_v8%': 0,  # enable the vtune support for V8 engine.
    'directxsdk_exists': '<!pymod_do_main(dir_exists ../third_party/directxsdk)',
  },
  'target_defaults': {
    'defines': ['CONTENT_IMPLEMENTATION'],
    'conditions': [
      # TODO(jschuh): Remove this after crbug.com/173851 gets fixed.
      ['OS=="win" and target_arch=="x64"', {
        'msvs_settings': {
          'VCCLCompilerTool': {
            'AdditionalOptions': ['/bigobj'],
          },
        },
      }],
    ],
  },
  'conditions': [
    ['OS != "ios"', {
      'includes': [
        'content_common_mojo_bindings.gypi',
        '../build/win_precompile.gypi',
      ],
    }],
    # In component mode, we build all of content as a single DLL.
    # However, in the static mode, we need to build content as multiple
    # targets in order to prevent dependencies from getting introduced
    # upstream unnecessarily (e.g., content_renderer depends on allocator
    # and chrome_exe depends on content_common but we don't want
    # chrome_exe to have to depend on allocator).
    ['component=="static_library"', {
      'target_defines': [
        'COMPILE_CONTENT_STATICALLY',
      ],
      'targets': [
        {
          'target_name': 'content',
          'type': 'none',
          'dependencies': [
            'content_browser',
            'content_common',
          ],
          'conditions': [
            ['OS != "ios"', {
              'dependencies': [
                'content_child',
                'content_gpu',
                'content_plugin',
                'content_ppapi_plugin',
                'content_renderer',
                'content_utility',
                'content_worker',
              ],
            }],
          ],
        },
        {
          'target_name': 'content_app_browser',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'includes': [
            'content_app.gypi',
          ],
          'dependencies': [
            'content_common',
          ],
          'conditions': [
            ['chrome_multiple_dll', {
              'defines': [
                'CHROME_MULTIPLE_DLL_BROWSER',
              ],
            }],
          ],
        },
        {
          'target_name': 'content_app_child',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'includes': [
            'content_app.gypi',
          ],
          'dependencies': [
            'content_common',
          ],
          'conditions': [
            ['chrome_multiple_dll', {
              'defines': [
                'CHROME_MULTIPLE_DLL_CHILD',
              ],
            }],
          ],
        },
        {
          'target_name': 'content_app_both',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'includes': [
            'content_app.gypi',
          ],
          'dependencies': [
            'content_common',
          ],
          'export_dependent_settings': [
            'content_common',
          ],
        },
        {
          'target_name': 'content_browser',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'includes': [
            'content_browser.gypi',
          ],
          'dependencies': [
            'content_common',
            'content_resources.gyp:content_resources',
          ],
          'conditions': [
            ['java_bridge==1', {
              'dependencies': [
                'content_child',
              ]
            }],
            ['OS=="android"', {
              'dependencies': [
                'content_gpu',
                'content_utility',
              ],
            }],
          ],
        },
        {
          'target_name': 'content_common',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'includes': [
            'content_common.gypi',
          ],
          'conditions': [
            ['OS != "ios"', {
              'dependencies': [
                'content_resources.gyp:content_resources',
              ],
            }],
          ],
          # Disable c4267 warnings until we fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        },
      ],
      'conditions': [
        ['OS != "ios"', {
          'targets': [
            {
              'target_name': 'content_child',
              'type': 'static_library',
              'variables': { 'enable_wexit_time_destructors': 1, },
              'includes': [
                'content_child.gypi',
              ],
              'dependencies': [
                'content_resources.gyp:content_resources',
              ],
              # Disable c4267 warnings until we fix size_t to int truncations.
              'msvs_disabled_warnings': [ 4267, ],
            },
            {
              'target_name': 'content_gpu',
              'type': 'static_library',
              'variables': { 'enable_wexit_time_destructors': 1, },
              'includes': [
                'content_gpu.gypi',
              ],
              'dependencies': [
                'content_child',
                'content_common',
              ],
            },
            {
              'target_name': 'content_plugin',
              'type': 'static_library',
              'variables': { 'enable_wexit_time_destructors': 1, },
              'includes': [
                'content_plugin.gypi',
              ],
              'dependencies': [
                'content_child',
                'content_common',
              ],
            },
            {
              'target_name': 'content_ppapi_plugin',
              'type': 'static_library',
              'variables': { 'enable_wexit_time_destructors': 1, },
              'includes': [
                'content_ppapi_plugin.gypi',
              ],
              # Disable c4267 warnings until we fix size_t to int truncations.
              'msvs_disabled_warnings': [ 4267, ],
            },
            {
              'target_name': 'content_renderer',
              'type': 'static_library',
              'variables': { 'enable_wexit_time_destructors': 1, },
              'includes': [
                'content_renderer.gypi',
              ],
              'dependencies': [
                'content_child',
                'content_common',
                'content_resources.gyp:content_resources',
              ],
              'conditions': [
                ['chromium_enable_vtune_jit_for_v8==1', {
                  'dependencies': [
                    '../v8/src/third_party/vtune/v8vtune.gyp:v8_vtune',
                  ],
                }],
              ],
            },
            {
              'target_name': 'content_utility',
              'type': 'static_library',
              'variables': { 'enable_wexit_time_destructors': 1, },
              'includes': [
                'content_utility.gypi',
              ],
              'dependencies': [
                'content_child',
                'content_common',
              ],
            },
            {
              'target_name': 'content_worker',
              'type': 'static_library',
              'variables': { 'enable_wexit_time_destructors': 1, },
              'includes': [
                'content_worker.gypi',
              ],
              'dependencies': [
                'content_child',
                'content_common',
              ],
            },
          ],
        }],
      ],
    },
    {  # component != static_library
      'targets': [
        {
          'target_name': 'content',
          'type': 'shared_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
           'content_resources.gyp:content_resources',
          ],
          'conditions': [
            ['chromium_enable_vtune_jit_for_v8==1', {
              'dependencies': [
                '../v8/src/third_party/vtune/v8vtune.gyp:v8_vtune',
              ],
            }],
          ],
          'includes': [
            'content_app.gypi',
            'content_browser.gypi',
            'content_child.gypi',
            'content_common.gypi',
            'content_gpu.gypi',
            'content_plugin.gypi',
            'content_ppapi_plugin.gypi',
            'content_renderer.gypi',
            'content_utility.gypi',
            'content_worker.gypi',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'conditions': [
                ['incremental_chrome_dll==1', {
                  'UseLibraryDependencyInputs': "true",
                }],
              ],
            },
          },
        },
        {
          'target_name': 'content_app_browser',
          'type': 'none',
          'dependencies': ['content', 'content_browser'],
        },
        {
          'target_name': 'content_app_child',
          'type': 'none',
          'dependencies': ['content', 'content_child'],
        },
        {
          'target_name': 'content_app_both',
          'type': 'none',
          'dependencies': ['content'],
          'export_dependent_settings': ['content'],
        },
        {
          'target_name': 'content_browser',
          'type': 'none',
          'dependencies': ['content'],
          'export_dependent_settings': ['content'],
        },
        {
          'target_name': 'content_common',
          'type': 'none',
          'dependencies': ['content', 'content_resources.gyp:content_resources'],
          # Disable c4267 warnings until we fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
          'export_dependent_settings': ['content'],
        },
        {
          'target_name': 'content_child',
          'type': 'none',
          'dependencies': ['content'],
        },
        {
          'target_name': 'content_gpu',
          'type': 'none',
          'dependencies': ['content'],
        },
        {
          'target_name': 'content_plugin',
          'type': 'none',
          'dependencies': ['content'],
        },
        {
          'target_name': 'content_ppapi_plugin',
          'type': 'none',
          'dependencies': ['content'],
          # Disable c4267 warnings until we fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        },
        {
          'target_name': 'content_renderer',
          'type': 'none',
          'dependencies': ['content'],
        },
        {
          'target_name': 'content_utility',
          'type': 'none',
          'dependencies': ['content'],
          'export_dependent_settings': ['content'],
        },
        {
          'target_name': 'content_worker',
          'type': 'none',
          'dependencies': ['content'],
        },
      ],
    }],
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'common_aidl',
          'type': 'none',
          'variables': {
            'aidl_interface_file': 'public/android/java/src/org/chromium/content/common/common.aidl',
            'aidl_import_include': 'public/android/java/src',
          },
          'sources': [
            'public/android/java/src/org/chromium/content/common/IChildProcessCallback.aidl',
            'public/android/java/src/org/chromium/content/common/IChildProcessService.aidl',
          ],
          'includes': [ '../build/java_aidl.gypi' ],
        },
        {
          'target_name': 'content_java',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base',
            '../media/media.gyp:media_java',
            '../net/net.gyp:net',
            '../ui/android/ui_android.gyp:ui_java',
            'common_aidl',
            'content_common',
            'content_strings_grd',
            'content_gamepad_mapping',
            'gesture_event_type_java',
            'page_transition_types_java',
            'popup_item_type_java',
            'result_codes_java',
            'speech_recognition_error_java',
            'top_controls_state_java',
            'screen_orientation_values_java',
          ],
          'variables': {
            'java_in_dir': '../content/public/android/java',
            'has_java_resources': 1,
            'R_package': 'org.chromium.content',
            'R_package_relpath': 'org/chromium/content',
          },
          'conditions': [
            ['android_webview_build == 0', {
              'dependencies': [
                '../third_party/eyesfree/eyesfree.gyp:eyesfree_java',
                '../third_party/guava/guava.gyp:guava_javalib',
              ],
            }],
          ],
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'content_strings_grd',
          # The android_webview/Android.mk file depends on this target directly.
          'android_unmangled_name': 1,
          'type': 'none',
          'variables': {
            'grd_file': '../content/public/android/java/strings/android_content_strings.grd',
          },
          'includes': [
            '../build/java_strings_grd.gypi',
          ],
        },
        {
          'target_name': 'gesture_event_type_java',
          'type': 'none',
          'sources': [
            'public/android/java/src/org/chromium/content/browser/GestureEventType.template',
          ],
          'variables': {
            'package_name': 'org/chromium/content/browser',
            'template_deps': ['browser/android/gesture_event_type_list.h'],
          },
          'includes': [ '../build/android/java_cpp_template.gypi' ],
        },
        {
          'target_name': 'page_transition_types_java',
          'type': 'none',
          'sources': [
            'public/android/java/src/org/chromium/content/browser/PageTransitionTypes.template',
          ],
          'variables': {
            'package_name': 'org/chromium/content/browser',
            'template_deps': ['public/common/page_transition_types_list.h'],
          },
          'includes': [ '../build/android/java_cpp_template.gypi' ],
        },
        {
          'target_name': 'popup_item_type_java',
          'type': 'none',
          'sources': [
            'public/android/java/src/org/chromium/content/browser/input/PopupItemType.template',
          ],
          'variables': {
            'package_name': 'org/chromium/content/browser/input',
            'template_deps': ['browser/android/popup_item_type_list.h'],
          },
          'includes': [ '../build/android/java_cpp_template.gypi' ],
        },
        {
          'target_name': 'result_codes_java',
          'type': 'none',
          'sources': [
            'public/android/java/src/org/chromium/content/common/ResultCodes.template',
          ],
          'variables': {
            'package_name': 'org/chromium/content/common',
            'template_deps': ['public/common/result_codes_list.h'],
          },
          'includes': [ '../build/android/java_cpp_template.gypi' ],
        },
        {
          'target_name': 'speech_recognition_error_java',
          'type': 'none',
          'sources': [
            'public/android/java/src/org/chromium/content/browser/SpeechRecognitionError.template',
          ],
          'variables': {
            'package_name': 'org/chromium/content/browser',
            'template_deps': ['public/common/speech_recognition_error_list.h'],
          },
          'includes': [ '../build/android/java_cpp_template.gypi' ],
        },
        {
          'target_name': 'top_controls_state_java',
          'type': 'none',
          'sources': [
            'public/android/java/src/org/chromium/content/common/TopControlsState.template',
          ],
          'variables': {
            'package_name': 'org/chromium/content/common',
            'template_deps': ['public/common/top_controls_state_list.h'],
          },
          'includes': [ '../build/android/java_cpp_template.gypi' ],
        },
        {
          'target_name': 'screen_orientation_values_java',
          'type': 'none',
          'sources': [
            'public/android/java/src/org/chromium/content/common/ScreenOrientationValues.template',
          ],
          'variables': {
            'package_name': 'org/chromium/content/common',
            'template_deps': ['public/common/screen_orientation_values_list.h'],
          },
          'includes': [ '../build/android/java_cpp_template.gypi' ],
        },
        {
          'target_name': 'java_set_jni_headers',
          'type': 'none',
          'variables': {
            'jni_gen_package': 'content',
            'input_java_class': 'java/util/HashSet.class',
          },
          'includes': [ '../build/jar_file_jni_generator.gypi' ],
        },
        {
          'target_name': 'motionevent_jni_headers',
          'type': 'none',
          'variables': {
             'jni_gen_package': 'content',
             'input_java_class': 'android/view/MotionEvent.class',
           },
          'includes': [ '../build/jar_file_jni_generator.gypi' ],
        },
        {
          'target_name': 'content_jni_headers',
          'type': 'none',
          'dependencies': [
            'java_set_jni_headers',
            'motionevent_jni_headers'
          ],
          'includes': [ 'content_jni.gypi' ],
        },
        {
          'target_name': 'content_icudata',
          'type': 'none',
          'conditions': [
            ['icu_use_data_file_flag==1', {
              'copies': [
                {
                  'destination': '<(PRODUCT_DIR)/content_shell/assets',
                  'files': [
                    '<(PRODUCT_DIR)/icudtl.dat',
                  ],
                },
              ],
            }],
          ],
        },
        {
          'target_name': 'content_gamepad_mapping',
          'type': 'none',
          'sources': [
            'public/android/java/src/org/chromium/content/browser/input/CanonicalButtonIndex.template',
            'public/android/java/src/org/chromium/content/browser/input/CanonicalAxisIndex.template',
          ],
          'variables': {
            'package_name': 'org/chromium/content/browser/input',
            'template_deps': [
              'browser/gamepad/canonical_axis_index_list.h',
              'browser/gamepad/canonical_button_index_list.h',
            ],
          },
          'includes': [ '../build/android/java_cpp_template.gypi' ],
        },
      ],
    }],  # OS == "android"
  ],
}
