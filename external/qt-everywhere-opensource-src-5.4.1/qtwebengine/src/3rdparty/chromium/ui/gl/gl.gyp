# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },

  'targets': [
    {
      'target_name': 'gl',
      'type': '<(component)',
      'product_name': 'gl_wrapper',  # Avoid colliding with OS X's libGL.dylib
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/gpu/command_buffer/command_buffer.gyp:gles2_utils',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/mesa/mesa.gyp:mesa_headers',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
      ],
      'variables': {
        'gl_binding_output_dir': '<(SHARED_INTERMEDIATE_DIR)/ui/gl',
      },
      'defines': [
        'GL_IMPLEMENTATION',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/swiftshader/include',
        '<(DEPTH)/third_party/khronos',
        '<(DEPTH)/third_party/mesa/src/include',
        '<(gl_binding_output_dir)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(gl_binding_output_dir)',
        ],
      },
      'export_dependent_settings': [
        '<(DEPTH)/third_party/mesa/mesa.gyp:mesa_headers',
      ],
      'sources': [
        'android/gl_jni_registrar.cc',
        'android/gl_jni_registrar.h',
        'android/scoped_java_surface.cc',
        'android/scoped_java_surface.h',
        'android/surface_texture.cc',
        'android/surface_texture.h',
        'android/surface_texture_listener.cc',
        'android/surface_texture_listener.h',
        'android/surface_texture_tracker.cc',
        'android/surface_texture_tracker.h',
        'gl_bindings.h',
        'gl_bindings_skia_in_process.cc',
        'gl_bindings_skia_in_process.h',
        'gl_context.cc',
        'gl_context.h',
        'gl_context_android.cc',
        'gl_context_mac.mm',
        'gl_context_ozone.cc',
        'gl_context_osmesa.cc',
        'gl_context_osmesa.h',
        'gl_context_stub.cc',
        'gl_context_stub.h',
        'gl_context_stub_with_extensions.cc',
        'gl_context_stub_with_extensions.h',
        'gl_context_win.cc',
        'gl_context_x11.cc',
        'gl_export.h',
        'gl_fence.cc',
        'gl_fence.h',
        'gl_gl_api_implementation.cc',
        'gl_gl_api_implementation.h',
        'gl_image.cc',
        'gl_image.h',
        'gl_image_android.cc',
        'gl_image_mac.cc',
        'gl_image_ozone.cc',
        'gl_image_shm.cc',
        'gl_image_shm.h',
        'gl_image_stub.cc',
        'gl_image_stub.h',
        'gl_image_win.cc',
        'gl_image_x11.cc',
        'gl_implementation.cc',
        'gl_implementation.h',
        'gl_implementation_android.cc',
        'gl_implementation_ozone.cc',
        'gl_implementation_mac.cc',
        'gl_implementation_win.cc',
        'gl_implementation_x11.cc',
        'gl_osmesa_api_implementation.cc',
        'gl_osmesa_api_implementation.h',
        'gl_share_group.cc',
        'gl_share_group.h',
        'gl_state_restorer.cc',
        'gl_state_restorer.h',
        'gl_surface.cc',
        'gl_surface.h',
        'gl_surface_android.cc',
        'gl_surface_mac.cc',
        'gl_surface_stub.cc',
        'gl_surface_stub.h',
        'gl_surface_win.cc',
        'gl_surface_x11.cc',
        'gl_surface_osmesa.cc',
        'gl_surface_osmesa.h',
        'gl_surface_ozone.cc',
        'gl_switches.cc',
        'gl_switches.h',
        'gl_version_info.cc',
        'gl_version_info.h',
        'gpu_switching_manager.cc',
        'gpu_switching_manager.h',
        'scoped_binders.cc',
        'scoped_binders.h',
        'scoped_make_current.cc',
        'scoped_make_current.h',
        'sync_control_vsync_provider.cc',
        'sync_control_vsync_provider.h',
        '<(gl_binding_output_dir)/gl_bindings_autogen_gl.cc',
        '<(gl_binding_output_dir)/gl_bindings_autogen_gl.h',
        '<(gl_binding_output_dir)/gl_bindings_autogen_osmesa.cc',
        '<(gl_binding_output_dir)/gl_bindings_autogen_osmesa.h',
      ],
      # hard_dependency is necessary for this target because it has actions
      # that generate header files included by dependent targets. The header
      # files must be generated before the dependents are compiled. The usual
      # semantics are to allow the two targets to build concurrently.
      'hard_dependency': 1,
      'actions': [
        {
          'action_name': 'generate_gl_bindings',
          'variables': {
            'generator_path': 'generate_bindings.py',
            # Prefer khronos EGL/GLES headers by listing that path first.
            'header_paths': '../../third_party/khronos:../../third_party/mesa/src/include:.:../../gpu',
          },
          'inputs': [
            '<(generator_path)',
            '<!@(python <(generator_path) --header-paths=<(header_paths) --inputs)',
          ],
          'outputs': [
            '<(gl_binding_output_dir)/gl_bindings_autogen_egl.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_egl.h',
            '<(gl_binding_output_dir)/gl_bindings_api_autogen_egl.h',
            '<(gl_binding_output_dir)/gl_bindings_autogen_gl.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_gl.h',
            '<(gl_binding_output_dir)/gl_bindings_api_autogen_gl.h',
            '<(gl_binding_output_dir)/gl_bindings_autogen_glx.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_glx.h',
            '<(gl_binding_output_dir)/gl_bindings_api_autogen_glx.h',
            '<(gl_binding_output_dir)/gl_bindings_autogen_mock.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_mock.h',
            '<(gl_binding_output_dir)/gl_bindings_autogen_osmesa.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_osmesa.h',
            '<(gl_binding_output_dir)/gl_bindings_api_autogen_osmesa.h',
            '<(gl_binding_output_dir)/gl_bindings_autogen_wgl.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_wgl.h',
            '<(gl_binding_output_dir)/gl_bindings_api_autogen_wgl.h',
            '<(gl_binding_output_dir)/gl_mock_autogen_gl.h',
          ],
          'action': [
            'python',
            '<(generator_path)',
            '--header-paths=<(header_paths)',
            '<(gl_binding_output_dir)',
          ],
        },
      ],
      'conditions': [
        ['OS in ("win", "android", "linux")', {
          'sources': [
            'egl_util.cc',
            'egl_util.h',
            'gl_context_egl.cc',
            'gl_context_egl.h',
            'gl_image_egl.cc',
            'gl_image_egl.h',
            'gl_surface_egl.cc',
            'gl_surface_egl.h',
            'gl_egl_api_implementation.cc',
            'gl_egl_api_implementation.h',
            '<(gl_binding_output_dir)/gl_bindings_autogen_egl.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_egl.h',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/khronos',
        ],
        }],
        ['OS in ("android", "linux")', {
          'sources': [
            'gl_implementation_osmesa.cc',
            'gl_implementation_osmesa.h',
          ],
        }],
        ['use_x11 == 1', {
          'sources': [
            'gl_context_glx.cc',
            'gl_context_glx.h',
            'gl_glx_api_implementation.cc',
            'gl_glx_api_implementation.h',
            'gl_image_glx.cc',
            'gl_image_glx.h',
            'gl_surface_glx.cc',
            'gl_surface_glx.h',
            'gl_egl_api_implementation.cc',
            'gl_egl_api_implementation.h',
            '<(gl_binding_output_dir)/gl_bindings_autogen_glx.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_glx.h',
          ],
          'all_dependent_settings': {
            'defines': [
              'GL_GLEXT_PROTOTYPES',
            ],
          },
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:x11',
            '<(DEPTH)/build/linux/system.gyp:xcomposite',
            '<(DEPTH)/build/linux/system.gyp:xext',
            '<(DEPTH)/ui/events/platform/events_platform.gyp:events_platform',
            '<(DEPTH)/ui/gfx/x/gfx_x11.gyp:gfx_x11',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            'gl_context_wgl.cc',
            'gl_context_wgl.h',
            'gl_egl_api_implementation.cc',
            'gl_egl_api_implementation.h',
            'gl_surface_wgl.cc',
            'gl_surface_wgl.h',
            'gl_wgl_api_implementation.cc',
            'gl_wgl_api_implementation.h',
            '<(gl_binding_output_dir)/gl_bindings_autogen_wgl.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_wgl.h',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'DelayLoadDLLs': [
                'dwmapi.dll',
              ],
              'AdditionalDependencies': [
                'dwmapi.lib',
              ],
            },
          },
          'link_settings': {
            'libraries': [
              '-ldwmapi.lib',
            ],
          },
        }],
        ['OS=="mac"', {
          'sources': [
            'gl_context_cgl.cc',
            'gl_context_cgl.h',
            'gl_image_io_surface.cc',
            'gl_image_io_surface.h',
            'scoped_cgl.cc',
            'scoped_cgl.h',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/IOSurface.framework',
              '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
            ],
          },
        }],
        ['OS=="android"', {
          'dependencies': [
            'gl_jni_headers',
          ],
          'sources': [
            'gl_image_android_native_buffer.cc',
            'gl_image_android_native_buffer.h',
            'gl_image_surface_texture.cc',
            'gl_image_surface_texture.h',
          ],
          'link_settings': {
            'libraries': [
              '-landroid',
            ],
          },
          'sources!': [
            'system_monitor_posix.cc',
          ],
          'defines': [
            'GL_GLEXT_PROTOTYPES',
            'EGL_EGLEXT_PROTOTYPES',
          ],
        }],
        ['OS!="android"', {
          'sources/': [ ['exclude', '^android/'] ],
        }],
        ['use_ozone==1', {
          'dependencies': [
            '../ozone/ozone.gyp:ozone',
          ],
        }],
        ['OS=="android" and android_webview_build==0', {
          'dependencies': [
            '../android/ui_android.gyp:ui_java',
          ],
        }],
      ],
    },
    {
      'target_name': 'gl_unittest_utils',
      'type': 'static_library',
      'variables': {
        'gl_binding_output_dir': '<(SHARED_INTERMEDIATE_DIR)/ui/gl',
      },
      'dependencies': [
        '../../testing/gmock.gyp:gmock',
        '../../third_party/khronos/khronos.gyp:khronos_headers',
        'gl',
      ],
      'include_dirs': [
        '<(gl_binding_output_dir)',
        '../..',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(gl_binding_output_dir)',
        ],
      },
      'sources': [
        'gl_mock.h',
        'gl_mock.cc',
        '<(gl_binding_output_dir)/gl_bindings_autogen_mock.cc',
        '<(gl_binding_output_dir)/gl_bindings_autogen_mock.h',
        '<(gl_binding_output_dir)/gl_mock_autogen_gl.h',
      ],
    },
  ],
  'conditions': [
    ['OS=="android"' , {
      'targets': [
        {
          'target_name': 'surface_jni_headers',
          'type': 'none',
          'variables': {
            'jni_gen_package': 'ui/gl',
            'input_java_class': 'android/view/Surface.class',
          },
          'includes': [ '../../build/jar_file_jni_generator.gypi' ],
        },
        {
          'target_name': 'gl_jni_headers',
          'type': 'none',
          'dependencies': [
            'surface_jni_headers',
          ],
          'sources': [
            '../android/java/src/org/chromium/ui/gl/SurfaceTexturePlatformWrapper.java',
            '../android/java/src/org/chromium/ui/gl/SurfaceTextureListener.java',
          ],
          'variables': {
            'jni_gen_package': 'ui/gl',
          },
          'includes': [ '../../build/jni_generator.gypi' ],
        },
      ],
    }],
  ],
}
