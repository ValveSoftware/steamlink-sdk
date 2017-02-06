# Copyright 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
    'system_webview_package_name%': 'com.android.webview',
  },
  'targets': [
    {
      'target_name': 'android_webview_pak',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/content/app/resources/content_resources.gyp:content_resources',
        '<(DEPTH)/net/net.gyp:net_resources',
        '<(DEPTH)/third_party/WebKit/public/blink_resources.gyp:blink_resources',
        '<(DEPTH)/ui/resources/ui_resources.gyp:ui_resources',
      ],
      'variables': {
        'conditions': [
          ['target_arch=="arm" or target_arch=="ia32" or target_arch=="mipsel"', {
            'arch_suffix':'32'
          }],
          ['target_arch=="arm64" or target_arch=="x64" or target_arch=="mips64el"', {
            'arch_suffix':'64'
          }],
        ],
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/android_webview',
      },
      'actions': [
        # GN version: //android_webview:generate_aw_resources
        {
          'action_name': 'generate_aw_resources',
          'variables': {
            'grit_grd_file': 'ui/aw_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        # GN version: //android_webview:repack_pack
        {
          'action_name': 'repack_android_webview_pack',
          'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/blink/public/resources/blink_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/blink/public/resources/blink_image_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/app/resources/content_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_resources_100_percent.pak',
              '<(grit_out_dir)/aw_resources.pak',
            ],
            'pak_output': '<(webview_chromium_pak_path)',
          },
         'includes': [ '../build/repack_action.gypi' ],
        },
        # GN version: //android_webview:generate_aw_strings
        {
          'action_name': 'generate_aw_strings',
          'variables': {
            'grit_grd_file': 'ui/aw_strings.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        # GN version: //android_webview:generate_components_strings
        {
          'action_name': 'generate_components_strings',
          'variables': {
             # components_strings contains strings from all components. WebView
             # will never display most of them, so we try to limit the included
             # strings
            'grit_whitelist': 'ui/grit_components_whitelist.txt',
            'grit_grd_file': '../components/components_strings.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        # GN Version: //android_webview:repack_locales
        {
          'action_name': 'android_webview_repack_locales',
          'variables': {
            'repack_locales': 'tools/webview_repack_locales.py',
          },
          'inputs': [
            '<(repack_locales)',
            '<!@pymod_do_main(webview_repack_locales -i -p <(PRODUCT_DIR) -s <(SHARED_INTERMEDIATE_DIR) <(locales))'
          ],
          'outputs': [
            '<!@pymod_do_main(webview_repack_locales -o -p <(PRODUCT_DIR) -s <(SHARED_INTERMEDIATE_DIR) <(locales))'
          ],
          'action': [
            'python',
            '<(repack_locales)',
            '-p', '<(PRODUCT_DIR)',
            '-s', '<(SHARED_INTERMEDIATE_DIR)',
            '<@(locales)',
          ],
        },
        # GN version:  //android_webview/rename_snapshot_blob
        {
          'action_name': 'rename_snapshot_blob',
          'inputs': [
            '<(PRODUCT_DIR)/snapshot_blob.bin',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/snapshot_blob_<(arch_suffix).bin',
          ],
          'action': [
            'python',
            '<(DEPTH)/build/cp.py',
            '<@(_inputs)',
            '<@(_outputs)',
          ],
        },
        # GN version:  //android_webview/rename_natives_blob
        {
          'action_name': 'rename_natives_blob',
          'inputs': [
            '<(PRODUCT_DIR)/natives_blob.bin',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/natives_blob_<(arch_suffix).bin',
          ],
          'action': [
            'python',
            '<(DEPTH)/build/cp.py',
            '<@(_inputs)',
            '<@(_outputs)',
          ],
        },
        # GN version:  //android_webview/generate_webview_license_notice
        {
          'action_name': 'generate_webview_license_notice',
          'inputs': [
            '<!@(python <(DEPTH)/android_webview/tools/webview_licenses.py notice_deps)',
            '<(DEPTH)/android_webview/tools/licenses_notice.tmpl',
            '<(DEPTH)/android_webview/tools/webview_licenses.py',
          ],
          'outputs': [
             '<(webview_licenses_path)',
          ],
          'action': [
            'python',
              '<(DEPTH)/android_webview/tools/webview_licenses.py',
              'notice',
              '<(webview_licenses_path)',
          ],
          'message': 'Generating WebView license notice',
        },
      ],
    },
    # GN version:  //android_webview/locale_paks
    {
      'target_name': 'android_webview_locale_paks',
      'type': 'none',
      'variables': {
        'locale_pak_files': [
          '<@(webview_locales_input_common_paks)',
          '<@(webview_locales_input_individual_paks)',
        ],
      },
      'includes': [
        '../build/android/locale_pak_resources.gypi',
      ],
    },
    {
      # GN version:  //android_webview:strings_grd
      'target_name': 'android_webview_strings_grd',
      'android_unmangled_name': 1,
      'type': 'none',
      'variables': {
        'grd_file': '../android_webview/java/strings/android_webview_strings.grd',
      },
      'includes': [
          '../build/java_strings_grd.gypi',
      ],
    },
    {
      # GN version:  //android_webview/common:version
      'target_name': 'android_webview_version',
      'type': 'none',
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
      # Because generate_version generates a header, we must set the
      # hard_dependency flag.
      'hard_dependency': 1,
      'actions': [
        {
          'action_name': 'generate_version',
          'includes': [
            '../build/util/version.gypi',
          ],
          'variables': {
            'template_input_path': 'common/aw_version_info_values.h.version',
          },
          'inputs': [
            '<(version_py_path)',
            '<(template_input_path)',
            '<(version_path)',
            '<(lastchange_path)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/android_webview/common/aw_version_info_values.h',
          ],
          'action': [
            'python',
            '<(version_py_path)',
            '-f', '<(version_path)',
            '-f', '<(lastchange_path)',
            '<(template_input_path)',
            '<@(_outputs)',
          ],
          'message': 'Generating version information',
        },
      ],
    },
    # GN version: //android_webview:common
    {
      'target_name': 'android_webview_common',
      'type': 'static_library',
      'dependencies': [
        '../android_webview/native/webview_native.gyp:webview_native',
        '../cc/cc.gyp:cc_surfaces',
        '../components/components.gyp:auto_login_parser',
        '../components/components.gyp:autofill_content_renderer',
        '../components/components.gyp:breakpad_host',
        '../components/components.gyp:cdm_browser',
        '../components/components.gyp:cdm_renderer',
        '../components/components.gyp:component_metrics_proto',
        '../components/components.gyp:crash_component',
        '../components/components.gyp:devtools_discovery',
        '../components/components.gyp:metrics',
        '../components/components.gyp:metrics_gpu',
        '../components/components.gyp:metrics_net',
        '../components/components.gyp:metrics_profiler',
        '../components/components.gyp:metrics_ui',
        '../components/components.gyp:navigation_interception',
        '../components/components.gyp:network_session_configurator_switches',
        '../components/components.gyp:printing_common',
        '../components/components.gyp:printing_browser',
        '../components/components.gyp:printing_renderer',
        '../components/components.gyp:visitedlink_browser',
        '../components/components.gyp:visitedlink_renderer',
        '../components/components.gyp:web_contents_delegate_android',
        '../content/content.gyp:content_app_both',
        '../content/content.gyp:content_browser',
        '../gin/gin.gyp:gin',
        '../gpu/command_buffer/command_buffer.gyp:gles2_utils',
        '../gpu/gpu.gyp:command_buffer_service',
        '../gpu/gpu.gyp:gl_in_process_context',
        '../gpu/gpu.gyp:gles2_c_lib',
        '../gpu/gpu.gyp:gles2_implementation',
        '../gpu/skia_bindings/skia_bindings.gyp:gpu_skia_bindings',
        '../media/media.gyp:media',
        '../media/midi/midi.gyp:midi',
         '../net/net.gyp:net_extras',
        '../printing/printing.gyp:printing',
        '../skia/skia.gyp:skia',
        '../third_party/WebKit/public/blink.gyp:blink',
        '../ui/events/events.gyp:gesture_detection',
        '../ui/gl/gl.gyp:gl',
        '../ui/gl/init/gl_init.gyp:gl_init',
        '../ui/shell_dialogs/shell_dialogs.gyp:shell_dialogs',
        '../url/ipc/url_ipc.gyp:url_ipc',
        '../v8/src/v8.gyp:v8',
        'android_webview_pak',
        'android_webview_version',
        '../components/components.gyp:pref_registry',
        '../components/url_formatter/url_formatter.gyp:url_formatter',
      ],
      'include_dirs': [
        '..',
        '../skia/config',
        '<(SHARED_INTERMEDIATE_DIR)/ui/resources/',
      ],
      'sources': [
        'browser/aw_browser_context.cc',
        'browser/aw_browser_context.h',
        'browser/aw_browser_main_parts.cc',
        'browser/aw_browser_main_parts.h',
        'browser/aw_browser_permission_request_delegate.h',
        'browser/aw_browser_policy_connector.cc',
        'browser/aw_browser_policy_connector.h',
        'browser/aw_content_browser_client.cc',
        'browser/aw_content_browser_client.h',
        'browser/aw_contents_client_bridge_base.cc',
        'browser/aw_contents_client_bridge_base.h',
        'browser/aw_contents_io_thread_client.h',
        'browser/aw_cookie_access_policy.cc',
        'browser/aw_cookie_access_policy.h',
        'browser/aw_dev_tools_discovery_provider.cc',
        'browser/aw_dev_tools_discovery_provider.h',
        'browser/aw_download_manager_delegate.cc',
        'browser/aw_download_manager_delegate.h',
        'browser/aw_form_database_service.cc',
        'browser/aw_form_database_service.h',
        'browser/aw_gl_surface.cc',
        'browser/aw_gl_surface.h',
        'browser/aw_http_auth_handler_base.cc',
        'browser/aw_http_auth_handler_base.h',
        'browser/aw_javascript_dialog_manager.cc',
        'browser/aw_javascript_dialog_manager.h',
        'browser/aw_locale_manager.h',
        'browser/aw_login_delegate.cc',
        'browser/aw_login_delegate.h',
        'browser/aw_message_port_message_filter.cc',
        'browser/aw_message_port_message_filter.h',
        'browser/aw_message_port_service.h',
        'browser/aw_metrics_service_client.cc',
        'browser/aw_metrics_service_client.h',
        'browser/aw_permission_manager.cc',
        'browser/aw_permission_manager.h',
        'browser/aw_print_manager.cc',
        'browser/aw_print_manager.h',
        'browser/aw_printing_message_filter.cc',
        'browser/aw_printing_message_filter.h',
        'browser/aw_quota_manager_bridge.cc',
        'browser/aw_quota_manager_bridge.h',
        'browser/aw_quota_permission_context.cc',
        'browser/aw_quota_permission_context.h',
        'browser/aw_render_thread_context_provider.cc',
        'browser/aw_render_thread_context_provider.h',
        'browser/aw_resource_context.cc',
        'browser/aw_resource_context.h',
        'browser/aw_result_codes.h',
        'browser/aw_ssl_host_state_delegate.cc',
        'browser/aw_ssl_host_state_delegate.h',
        'browser/aw_web_preferences_populater.cc',
        'browser/aw_web_preferences_populater.h',
        'browser/browser_view_renderer.cc',
        'browser/browser_view_renderer.h',
        'browser/browser_view_renderer_client.h',
        'browser/child_frame.cc',
        'browser/child_frame.h',
        'browser/compositor_id.cc',
        'browser/compositor_id.h',
        'browser/deferred_gpu_command_service.cc',
        'browser/deferred_gpu_command_service.h',
        'browser/find_helper.cc',
        'browser/find_helper.h',
        'browser/gl_view_renderer_manager.cc',
        'browser/gl_view_renderer_manager.h',
        'browser/hardware_renderer.cc',
        'browser/hardware_renderer.h',
        'browser/icon_helper.cc',
        'browser/icon_helper.h',
        'browser/input_stream.h',
        'browser/jni_dependency_factory.h',
        'browser/net/android_stream_reader_url_request_job.cc',
        'browser/net/android_stream_reader_url_request_job.h',
        'browser/net/aw_cookie_store_wrapper.cc',
        'browser/net/aw_cookie_store_wrapper.h',
        'browser/net/aw_http_user_agent_settings.cc',
        'browser/net/aw_http_user_agent_settings.h',
        'browser/net/aw_network_change_notifier.cc',
        'browser/net/aw_network_change_notifier.h',
        'browser/net/aw_network_change_notifier_factory.cc',
        'browser/net/aw_network_change_notifier_factory.h',
        'browser/net/aw_network_delegate.cc',
        'browser/net/aw_network_delegate.h',
        'browser/net/aw_request_interceptor.cc',
        'browser/net/aw_request_interceptor.h',
        'browser/net/aw_url_request_context_getter.cc',
        'browser/net/aw_url_request_context_getter.h',
        'browser/net/aw_url_request_job_factory.cc',
        'browser/net/aw_url_request_job_factory.h',
        'browser/net/aw_web_resource_response.h',
        'browser/net/init_native_callback.h',
        'browser/net/input_stream_reader.cc',
        'browser/net/input_stream_reader.h',
        'browser/net/token_binding_manager.cc',
        'browser/net/token_binding_manager.h',
        'browser/net_disk_cache_remover.cc',
        'browser/net_disk_cache_remover.h',
        'browser/parent_compositor_draw_constraints.cc',
        'browser/parent_compositor_draw_constraints.h',
        'browser/parent_output_surface.cc',
        'browser/parent_output_surface.h',
        'browser/render_thread_manager.cc',
        'browser/render_thread_manager.h',
        'browser/renderer_host/aw_render_view_host_ext.cc',
        'browser/renderer_host/aw_render_view_host_ext.h',
        'browser/renderer_host/aw_resource_dispatcher_host_delegate.cc',
        'browser/renderer_host/aw_resource_dispatcher_host_delegate.h',
        'browser/scoped_allow_wait_for_legacy_web_view_api.h',
        'browser/scoped_app_gl_state_restore.cc',
        'browser/scoped_app_gl_state_restore.h',
        'common/android_webview_message_generator.cc',
        'common/android_webview_message_generator.h',
        'common/aw_content_client.cc',
        'common/aw_content_client.h',
        'common/aw_descriptors.h',
        'common/aw_hit_test_data.cc',
        'common/aw_hit_test_data.h',
        'common/aw_media_client_android.cc',
        'common/aw_media_client_android.h',
        'common/aw_message_port_messages.h',
        'common/aw_resource.h',
        'common/aw_switches.cc',
        'common/aw_switches.h',
        'common/devtools_instrumentation.h',
        'common/render_view_messages.cc',
        'common/render_view_messages.h',
        'common/url_constants.cc',
        'common/url_constants.h',
        'crash_reporter/aw_microdump_crash_reporter.cc',
        'crash_reporter/aw_microdump_crash_reporter.h',
        'gpu/aw_content_gpu_client.cc',
        'gpu/aw_content_gpu_client.h',
        'lib/aw_browser_dependency_factory_impl.cc',
        'lib/aw_browser_dependency_factory_impl.h',
        'lib/main/aw_main_delegate.cc',
        'lib/main/aw_main_delegate.h',
        'lib/main/webview_jni_onload.cc',
        'lib/main/webview_jni_onload.h',
        'public/browser/draw_gl.h',
        'renderer/aw_content_renderer_client.cc',
        'renderer/aw_content_renderer_client.h',
        'renderer/aw_content_settings_client.cc',
        'renderer/aw_content_settings_client.h',
        'renderer/aw_key_systems.cc',
        'renderer/aw_key_systems.h',
        'renderer/aw_message_port_client.cc',
        'renderer/aw_message_port_client.h',
        'renderer/aw_print_web_view_helper_delegate.cc',
        'renderer/aw_print_web_view_helper_delegate.h',
        'renderer/aw_render_frame_ext.cc',
        'renderer/aw_render_frame_ext.h',
        'renderer/aw_render_thread_observer.cc',
        'renderer/aw_render_thread_observer.h',
        'renderer/aw_render_view_ext.cc',
        'renderer/aw_render_view_ext.h',
        'renderer/print_render_frame_observer.cc',
        'renderer/print_render_frame_observer.h',
      ],
      'conditions': [
        ['configuration_policy==1', {
          'dependencies': [
            '../components/components.gyp:policy',
            '../components/components.gyp:policy_component',
          ],
        }],
      ],
    },
    # GN version:  //android_webview:libwebviewchromium
    {
      'target_name': 'libwebviewchromium',
      'includes': [
          'libwebviewchromium.gypi',
      ],
    },
    {
      # GN version:  //android_webview:android_webview_java
      'target_name': 'android_webview_java',
      'type': 'none',
      'dependencies': [
        '../android_webview/native/webview_native.gyp:android_webview_aw_permission_request_resource',
        '../components/components.gyp:external_video_surface_java',
        '../components/components.gyp:navigation_interception_java',
        '../components/components.gyp:web_contents_delegate_android_java',
        '../content/content.gyp:content_java',
        '../ui/android/ui_android.gyp:ui_java',
        'android_webview_locale_paks',
        'android_webview_strings_grd',
      ],
      'variables': {
        'java_in_dir': '../android_webview/java',
        'has_java_resources': 1,
        'R_package': 'org.chromium.android_webview',
        'R_package_relpath': 'org/chromium/android_webview',
        # for lint; do not use the system webview's manifest because it's a template
        'android_manifest_path': '../android_webview/test/shell/AndroidManifest.xml',
      },
      'conditions': [
        ['configuration_policy==1', {
          'dependencies': [
            '../components/components.gyp:policy_java',
          ],
        }],
      ],
      'includes': [ '../build/java.gypi' ],
    },
    # GN version:  //android_webview/glue
    {
      'target_name': 'system_webview_glue_java',
      'variables': {
        'android_sdk_jar': '../third_party/android_platform/webview/frameworks_6.0.jar',
        'java_in_dir': 'glue/java',
      },
      'includes': [ 'apk/system_webview_glue_common.gypi' ],
    },
  ],
  'conditions': [
    ['use_webview_internal_framework==0', {
      'targets': [
        # GN version:  //android_webview:system_webview_apk
        {
          'target_name': 'system_webview_apk',
          'dependencies': [
            'system_webview_glue_java',
          ],
          'variables': {
            'apk_name': 'SystemWebView',
            'android_sdk_jar': '../third_party/android_platform/webview/frameworks_6.0.jar',
            'java_in_dir': '../build/android/empty',
            'resource_dir': 'apk/java/res',
            'android_manifest_template_vars': ['package=<(system_webview_package_name)'],
          },
          'includes': [ 'apk/system_webview_apk_common.gypi' ],
        },
      ],
    }]
  ],
  'includes': [
    'android_webview_tests.gypi',
    'apk/system_webview_paks.gypi',
  ],
}
