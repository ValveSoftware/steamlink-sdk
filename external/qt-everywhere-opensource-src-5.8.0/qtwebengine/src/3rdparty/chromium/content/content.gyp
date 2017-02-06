# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,  # Use higher warning level.
    'chromium_enable_vtune_jit_for_v8%': 0,  # enable the vtune support for V8 engine.
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
  'targets': [
    {
      # GN version: //content/public/common:features
      'target_name': 'common_features',
      'includes': [
        '../build/buildflag_header.gypi',
        '../third_party/webrtc/build/common.gypi',
      ],
      'conditions': [
        # This conditional looks insane, but without it |rtc_use_h264| is not
        # recognized as defined. Might have something to do with scopes. Moving
        # the inclusion of third_party/webrtc/build/common.gypi to outside of
        # 'targets' is not an option, then we get compile errors.
        # TODO(hbos): crbug.com/584219
        ['1==1', {
          'variables': {
            'buildflag_header_path': 'content/public/common/features.h',
            'buildflag_flags': [
              'RTC_USE_H264=<(rtc_use_h264)',
            ],
          },
        }],
      ],
    },
    {
      # GN version: //content/public/common:feature_h264_with_openh264_ffmpeg
      'target_name': 'feature_h264_with_openh264_ffmpeg',
      'type': 'static_library',
      'include_dirs': [ '<@(DEPTH)' ],
      'dependencies': [
        'common_features',
      ],
      'sources': [
        'public/common/feature_h264_with_openh264_ffmpeg.cc',
        'public/common/feature_h264_with_openh264_ffmpeg.h',
      ],
    },
    {
      # GN version: //content/public/app:browser_manifest
      'target_name': 'content_app_browser_manifest',
      'type': 'none',
      'variables': {
        'application_type': 'exe',
        'application_name': 'content_browser',
        'packaged_manifests': [
          '<(PRODUCT_DIR)/Mojo Applications/user/manifest.json',
        ],
        'source_manifest': '<(DEPTH)/content/public/app/mojo/content_browser_manifest.json',
      },
      'includes': [
        '../mojo/public/mojo_application_manifest.gypi',
      ],
      'dependencies': [
        '../services/user/user.gyp:user_app_manifest',
      ],
      'hard_dependency': 1,
    },
    {
      # GN version: //content/public/app:gpu_manifest
      'target_name': 'content_app_gpu_manifest',
      'type': 'none',
      'variables': {
        'application_type': 'exe',
        'application_name': 'content_gpu',
        'source_manifest': '<(DEPTH)/content/public/app/mojo/content_gpu_manifest.json',
      },
      'includes': [
        '../mojo/public/mojo_application_manifest.gypi',
      ],
      'hard_dependency': 1,
    },
    {
      # GN version: //content/public/app:renderer_manifest
      'target_name': 'content_app_renderer_manifest',
      'type': 'none',
      'variables': {
        'application_type': 'exe',
        'application_name': 'content_renderer',
        'source_manifest': '<(DEPTH)/content/public/app/mojo/content_renderer_manifest.json',
      },
      'includes': [
        '../mojo/public/mojo_application_manifest.gypi',
      ],
      'hard_dependency': 1,
    },
    {
      # GN version: //content/public/app:utility_manifest
      'target_name': 'content_app_utility_manifest',
      'type': 'none',
      'variables': {
        'application_type': 'exe',
        'application_name': 'content_utility',
        'source_manifest': '<(DEPTH)/content/public/app/mojo/content_utility_manifest.json',
      },
      'includes': [
        '../mojo/public/mojo_application_manifest.gypi',
      ],
      'hard_dependency': 1,
    },
  ],
  'includes': [
    '../build/win_precompile.gypi',
    'content_resources.gypi',
  ],
  'conditions': [
    ['OS == "win"', {
      'targets': [
        {
          # GN: //content:sandbox_helper_win
          'target_name': 'sandbox_helper_win',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            '../sandbox/sandbox.gyp:sandbox',
          ],
          'sources': [
            'app/sandbox_helper_win.cc',
            'public/app/sandbox_helper_win.h',
          ],
        }
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
          # GN version: //content
          'target_name': 'content',
          'type': 'none',
          'dependencies': [
            'content_browser',
            'content_child',
            'content_common',
            'content_gpu',
            'content_ppapi_plugin',
            'content_renderer',
            'content_utility',
          ],
          'export_dependent_settings': [
            'content_common',
          ],
        },
        {
          # GN version: //content/app:browser
          'target_name': 'content_app_browser',
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
          'conditions': [
            ['chrome_multiple_dll', {
              'defines': [
                'CHROME_MULTIPLE_DLL_BROWSER',
              ],
            }],
          ],
        },
        {
          # GN version: //content/app:child
          'target_name': 'content_app_child',
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
          'conditions': [
            ['chrome_multiple_dll', {
              'defines': [
                'CHROME_MULTIPLE_DLL_CHILD',
              ],
            }],
          ],
        },
        {
          # GN version: //content/app:both
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
          # GN version: //content/browser and //content/public/browser
          'target_name': 'content_browser',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'includes': [
            'content_browser.gypi',
            # Disable LTO due to ELF section name out of range
            # crbug.com/422251
            '../build/android/disable_gcc_lto.gypi',
          ],
          'dependencies': [
            'content_common',
            'content_gpu',
            'content_resources',
          ],
          'export_dependent_settings': [
            'content_common',
          ],
          'conditions': [
            ['java_bridge==1', {
              'dependencies': [
                'content_child',
              ]
            }],
            ['OS=="android"', {
              'dependencies': [
                'content_utility',
              ],
            }],
            # Shard this target into parts to work around linker limitations
            # on link time code generation builds. See crbug.com/616946
            ['OS=="win" and buildtype=="Official"', {
              'msvs_shard': 5,
            }],
          ],
        },
        {
          # GN version: //content/common and //content/public/common
          'target_name': 'content_common',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            'content_resources',
          ],
          'includes': [
            'content_common.gypi',
          ],
          # Disable c4267 warnings until we fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        },
        {
              # GN version: //content/child and //content/public/child
          'target_name': 'content_child',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'includes': [
            'content_child.gypi',
          ],
          'dependencies': [
            'content_resources',
          ],
          # Disable c4267 warnings until we fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        },
        {
          # GN version: //content/gpu
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
          # GN version: //content/ppapi_plugin
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
          # GN version: //content/renderer and //content/public/renderer
          'target_name': 'content_renderer',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'includes': [
            'content_renderer.gypi',
            '../third_party/webrtc/build/common.gypi',
          ],
          'dependencies': [
            'common_features',
            'content_child',
            'content_common',
            'content_gpu',
            'content_resources',
          ],
          'export_dependent_settings': [
            'content_common',
          ],
          'conditions': [
            ['chromium_enable_vtune_jit_for_v8==1', {
              'dependencies': [
                '../v8/src/third_party/vtune/v8vtune.gyp:v8_vtune',
              ],
            }],
            ['rtc_use_h264==1', {
              'dependencies': [
                  '../third_party/openh264/openh264.gyp:openh264_encoder',
                  '../third_party/webrtc/modules/modules.gyp:webrtc_h264',
              ],
            }],
          ],
        },
        {
          # GN version: //content/utility and //content/public/utility
          'target_name': 'content_utility',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'includes': [
            'content_utility.gypi',
          ],
          'dependencies': [
            'content_child',
            'content_common',
            'content_common_mojo_bindings.gyp:content_common_mojo_bindings',
          ],
        },
      ],
    },
    {  # component != static_library
      'targets': [
        {
          # GN version: //content
          'target_name': 'content',
          'type': 'shared_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            'common_features',
            'content_resources',
          ],
          'conditions': [
            ['chromium_enable_vtune_jit_for_v8==1', {
              'dependencies': [
                '../v8/src/third_party/vtune/v8vtune.gyp:v8_vtune',
              ],
            }],
            ['rtc_use_h264==1', {
              'dependencies': [
                  '../third_party/openh264/openh264.gyp:openh264_encoder',
                  '../third_party/webrtc/modules/modules.gyp:webrtc_h264',
              ],
            }],
          ],
          'includes': [
            'content_app.gypi',
            'content_browser.gypi',
            'content_child.gypi',
            'content_common.gypi',
            'content_gpu.gypi',
            'content_ppapi_plugin.gypi',
            'content_renderer.gypi',
            'content_utility.gypi',
            '../third_party/webrtc/build/common.gypi',
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
          # GN version: //content/app:browser
          'target_name': 'content_app_browser',
          'type': 'none',
          'dependencies': ['content', 'content_browser'],
        },
        {
          # GN version: //content/app:child
          'target_name': 'content_app_child',
          'type': 'none',
          'dependencies': ['content', 'content_child'],
        },
        {
          # GN version: //content/app:both
          'target_name': 'content_app_both',
          'type': 'none',
          'dependencies': ['content'],
          'export_dependent_settings': ['content'],
        },
        {
          # GN version: //content/browser and //content/public/browser
          'target_name': 'content_browser',
          'type': 'none',
          'dependencies': ['content'],
          'export_dependent_settings': ['content'],
        },
        {
          # GN version: //content/common and //content/public/common
          'target_name': 'content_common',
          'type': 'none',
          'dependencies': [
            'content',
            'content_resources'
          ],
          # Disable c4267 warnings until we fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
          'export_dependent_settings': ['content'],
        },
        {
          # GN Version: //content/child
          'target_name': 'content_child',
          'type': 'none',
          'dependencies': ['content'],
        },
        {
          # GN version: //content/gpu
          'target_name': 'content_gpu',
          'type': 'none',
          'dependencies': ['content'],
        },
        {
          # GN version: //content/ppapi_plugin
          'target_name': 'content_ppapi_plugin',
          'type': 'none',
          'dependencies': ['content'],
          # Disable c4267 warnings until we fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        },
        {
          # GN version: //content/renderer and //content/public/renderer
          'target_name': 'content_renderer',
          'type': 'none',
          'dependencies': ['content'],
          'export_dependent_settings': ['content'],
        },
        {
          # GN version: //content/utility
          'target_name': 'content_utility',
          'type': 'none',
          'dependencies': ['content'],
          'export_dependent_settings': ['content'],
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
            '../device/battery/battery.gyp:device_battery_java',
            '../device/bluetooth/bluetooth.gyp:device_bluetooth_java',
            '../device/usb/usb.gyp:device_usb_java',
            '../device/vibration/vibration.gyp:device_vibration_java',
            '../media/media.gyp:media_java',
            '../mojo/mojo_base.gyp:mojo_system_java',
            '../mojo/mojo_public.gyp:mojo_bindings_java',
            '../net/net.gyp:net',
            '../skia/skia.gyp:skia_mojo',
            '../third_party/android_tools/android_tools.gyp:android_support_v13_javalib',
            '../third_party/WebKit/public/blink_headers.gyp:blink_headers_java',
            '../ui/android/ui_android.gyp:ui_java',
            '../ui/gfx/gfx.gyp:mojo_geometry_bindings',
            '../ui/touch_selection/ui_touch_selection.gyp:selection_event_type_java',
            '../ui/touch_selection/ui_touch_selection.gyp:touch_handle_orientation_java',
            'common_aidl',
            'console_message_level_java',
            'content_common',
            'content_strings_grd',
            'content_gamepad_mapping',
            'device_sensors_consts_java',
            'sensor_manager_android_java',
            'gesture_event_type_java',
            'invalidate_types_java',
            'navigation_controller_java',
            'popup_item_type_java',
            'result_codes_java',
            'readback_response_java',
            'speech_recognition_error_java',
            'top_controls_state_java',
            'screen_orientation_values_java',
            'accessibility_java',
          ],
          'variables': {
            'java_in_dir': '../content/public/android/java',
            'has_java_resources': 1,
            'R_package': 'org.chromium.content',
            'R_package_relpath': 'org/chromium/content',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'console_message_level_java',
          'type': 'none',
          'variables': {
            'source_file': 'public/common/console_message_level.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
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
          'target_name': 'device_sensors_consts_java',
          'type': 'none',
          'variables': {
            'source_file': 'browser/device_sensors/device_sensors_consts.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
        {
          'target_name': 'sensor_manager_android_java',
          'type': 'none',
          'variables': {
            'source_file': 'browser/device_sensors/sensor_manager_android.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
        {
          'target_name': 'content_gamepad_mapping',
          'type': 'none',
          'variables': {
            'source_file': 'browser/gamepad/gamepad_standard_mappings.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
        {
          'target_name': 'gesture_event_type_java',
          'type': 'none',
          'variables': {
            'source_file': 'browser/android/gesture_event_type.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
        {
          'target_name': 'invalidate_types_java',
          'type': 'none',
          'variables': {
            'source_file': 'public/browser/invalidate_type.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
        {
          'target_name': 'navigation_controller_java',
          'type': 'none',
          'variables': {
            'source_file': 'public/browser/navigation_controller.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
        {
          'target_name': 'popup_item_type_java',
          'type': 'none',
          'variables': {
            'source_file': 'browser/android/content_view_core_impl.cc',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
        {
          'target_name': 'readback_response_java',
          'type': 'none',
          'variables': {
            'source_file': 'public/browser/readback_types.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
        {
          'target_name': 'result_codes_java',
          'type': 'none',
          'variables': {
            'source_file': 'public/common/result_codes.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
        {
          'target_name': 'speech_recognition_error_java',
          'type': 'none',
          'variables': {
            'source_file': 'public/common/speech_recognition_error.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
        {
          'target_name': 'top_controls_state_java',
          'type': 'none',
          'variables': {
            'source_file': 'public/common/top_controls_state.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
        {
          'target_name': 'screen_orientation_values_java',
          'type': 'none',
          'variables': {
            'source_file': 'public/common/screen_orientation_values.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
        {
          'target_name': 'accessibility_java',
          'type': 'none',
          'variables': {
            'source_file': 'browser/accessibility/browser_accessibility_manager_android.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
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
          'target_name': 'content_jni_headers',
          'type': 'none',
          'dependencies': [
            'java_set_jni_headers',
          ],
          'includes': [ 'content_jni.gypi' ],
          'conditions': [
            ['enable_webvr==1', {
              'sources': [
                'public/android/java/src/org/chromium/content/browser/input/CardboardVRDevice.java',
              ],
              'dependencies': [
                '../third_party/cardboard-java/cardboard.gyp:cardboard_jar',
              ],
            }],
          ],
        },
        {
          'target_name': 'content_shell_assets_copy',
          'type': 'none',
          'dependencies': ['content_shell_and_tests.gyp:content_shell_pak'],
          'variables': {
            'src_files': ['<(PRODUCT_DIR)/content_shell.pak'],
            'conditions': [
              ['v8_use_external_startup_data==1', {
                'dependencies': ['<(DEPTH)/v8/src/v8.gyp:v8_external_snapshot'],
                'renaming_sources': [
                  '<(PRODUCT_DIR)/natives_blob.bin',
                  '<(PRODUCT_DIR)/snapshot_blob.bin',
                ],
                'renaming_destinations': [
                  'natives_blob_<(arch_suffix).bin',
                  'snapshot_blob_<(arch_suffix).bin',
                ],
              }],
              ['icu_use_data_file_flag==1', {
                'dependencies': ['<(DEPTH)/third_party/icu/icu.gyp:icudata'],
                'src_files': ['<(PRODUCT_DIR)/icudtl.dat'],
              }],
            ],
            'dest_path': '<(PRODUCT_DIR)/content_shell/assets',
            'clear': 1,
          },
          'includes': [
            '../build/android/copy_ex.gypi',
            '../build/android/v8_external_startup_data_arch_suffix.gypi',
          ],
        },
      ],
    }],  # OS == "android"
  ],
}
