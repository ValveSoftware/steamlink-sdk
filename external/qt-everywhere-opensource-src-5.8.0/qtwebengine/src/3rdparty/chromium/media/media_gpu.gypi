{
  'variables': {
    'use_v4lplugin%': 0,
    'use_v4l2_codec%': 0,
  },
  'defines': [
    'MEDIA_GPU_IMPLEMENTATION'
  ],
  'dependencies': [
    '../base/base.gyp:base',
    '../gpu/gpu.gyp:gpu',
    '../media/media.gyp:media',
    '../ui/display/display.gyp:display_types',
    '../ui/gfx/gfx.gyp:gfx_geometry',
    '../ui/gl/gl.gyp:gl',
    '../ui/gl/init/gl_init.gyp:gl_init',
    '../ui/platform_window/platform_window.gyp:platform_window',
    '../third_party/khronos/khronos.gyp:khronos_headers',
  ],
  'sources': [
    'gpu/fake_video_decode_accelerator.cc',
    'gpu/fake_video_decode_accelerator.h',
    'gpu/gpu_video_accelerator_util.cc',
    'gpu/gpu_video_accelerator_util.h',
    'gpu/gpu_video_decode_accelerator_factory_impl.cc',
    'gpu/gpu_video_decode_accelerator_factory_impl.h',
    'gpu/gpu_video_decode_accelerator_helpers.h',
    'gpu/shared_memory_region.cc',
    'gpu/shared_memory_region.h',
  ],
  'include_dirs': [
    '..',
  ],
  'conditions': [
    ['OS=="mac"', {
      'dependencies': [
        '../media/media.gyp:media',
        '../content/app/resources/content_resources.gyp:content_resources',
        '../third_party/webrtc/common_video/common_video.gyp:common_video',
        '../ui/accelerated_widget_mac/accelerated_widget_mac.gyp:accelerated_widget_mac'
      ],
      'sources': [
        'gpu/vt_mac.h',
        'gpu/vt_video_decode_accelerator_mac.cc',
        'gpu/vt_video_decode_accelerator_mac.h',
        'gpu/vt_video_encode_accelerator_mac.cc',
        'gpu/vt_video_encode_accelerator_mac.h',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/AVFoundation.framework',
          '$(SDKROOT)/System/Library/Frameworks/CoreMedia.framework',
          '$(SDKROOT)/System/Library/Frameworks/CoreVideo.framework',
          '$(SDKROOT)/System/Library/Frameworks/IOSurface.framework',
          '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
          '$(SDKROOT)/usr/lib/libsandbox.dylib',
        ],
      },
      'variables': {
        'generate_stubs_script': '../tools/generate_stubs/generate_stubs.py',
        'extra_header': 'gpu/vt_stubs_header.fragment',
        'sig_files': ['gpu/vt.sig'],
        'outfile_type': 'posix_stubs',
        'stubs_filename_root': 'vt_stubs',
        'project_path': 'media/gpu',
        'intermediate_dir': '<(INTERMEDIATE_DIR)',
        'output_root': '<(SHARED_INTERMEDIATE_DIR)/vt_stubs',
      },
      'include_dirs': [
        '<(output_root)',
      ],
      'actions': [
        {
          'action_name': 'generate_stubs',
          'inputs': [
            '<(generate_stubs_script)',
            '<(extra_header)',
            '<@(sig_files)',
          ],
          'outputs': [
            '<(intermediate_dir)/<(stubs_filename_root).cc',
            '<(output_root)/<(project_path)/<(stubs_filename_root).h',
          ],
          'action': ['python',
                     '<(generate_stubs_script)',
                     '-i', '<(intermediate_dir)',
                     '-o', '<(output_root)/<(project_path)',
                     '-t', '<(outfile_type)',
                     '-e', '<(extra_header)',
                     '-s', '<(stubs_filename_root)',
                     '-p', '<(project_path)',
                     '<@(_inputs)',
          ],
          'process_outputs_as_sources': 1,
          'message': 'Generating VideoToolbox stubs for dynamic loading',
        },
      ],
    }],
    ['OS=="android"', {
      'dependencies': [
        '../media/media.gyp:media',
      ],
      'sources': [
        'gpu/android_copying_backing_strategy.cc',
        'gpu/android_copying_backing_strategy.h',
        'gpu/android_deferred_rendering_backing_strategy.cc',
        'gpu/android_deferred_rendering_backing_strategy.h',
        'gpu/android_video_decode_accelerator.cc',
        'gpu/android_video_decode_accelerator.h',
        'gpu/avda_codec_image.cc',
        'gpu/avda_codec_image.h',
        'gpu/avda_return_on_failure.h',
        'gpu/avda_shared_state.cc',
        'gpu/avda_shared_state.h',
        'gpu/avda_state_provider.h',
        'gpu/avda_surface_tracker.h',
        'gpu/avda_surface_tracker.cc',
      ],
    }],
    ['OS=="android" and enable_webrtc==1', {
      'dependencies': [
        '../third_party/libyuv/libyuv.gyp:libyuv',
      ],
      'sources': [
        'gpu/android_video_encode_accelerator.cc',
        'gpu/android_video_encode_accelerator.h',
      ],
    }],
    ['use_v4lplugin==1 and chromeos==1', {
      'direct_dependent_settings': {
        'defines': [
          'USE_LIBV4L2'
        ],
      },
      'defines': [
        'USE_LIBV4L2'
      ],
      'variables': {
        'generate_stubs_script': '../tools/generate_stubs/generate_stubs.py',
        'extra_header': 'gpu/v4l2_stub_header.fragment',
        'sig_files': ['gpu/v4l2.sig'],
        'outfile_type': 'posix_stubs',
        'stubs_filename_root': 'v4l2_stubs',
        'project_path': 'media/gpu',
        'intermediate_dir': '<(INTERMEDIATE_DIR)',
        'output_root': '<(SHARED_INTERMEDIATE_DIR)/v4l2',
      },
      'include_dirs': [
        '<(output_root)',
      ],
      'actions': [
        {
          'action_name': 'generate_stubs',
          'inputs': [
            '<(generate_stubs_script)',
            '<(extra_header)',
            '<@(sig_files)',
          ],
          'outputs': [
            '<(intermediate_dir)/<(stubs_filename_root).cc',
            '<(output_root)/<(project_path)/<(stubs_filename_root).h',
          ],
          'action': ['python',
            '<(generate_stubs_script)',
            '-i', '<(intermediate_dir)',
            '-o', '<(output_root)/<(project_path)',
            '-t', '<(outfile_type)',
            '-e', '<(extra_header)',
            '-s', '<(stubs_filename_root)',
            '-p', '<(project_path)',
            '<@(_inputs)',
          ],
          'process_outputs_as_sources': 1,
          'message': 'Generating libv4l2 stubs for dynamic loading',
        },
      ],
    }],
    ['chromeos==1', {
      'sources': [
        'gpu/accelerated_video_decoder.h',
        'gpu/h264_decoder.cc',
        'gpu/h264_decoder.h',
        'gpu/h264_dpb.cc',
        'gpu/h264_dpb.h',
        'gpu/vp8_decoder.cc',
        'gpu/vp8_decoder.h',
        'gpu/vp8_picture.cc',
        'gpu/vp8_picture.h',
        'gpu/vp9_decoder.cc',
        'gpu/vp9_decoder.h',
        'gpu/vp9_picture.cc',
        'gpu/vp9_picture.h',
      ],
    }],
    ['chromeos==1 and use_v4l2_codec==1', {
      'direct_dependent_settings': {
        'defines': [
          'USE_V4L2_CODEC'
        ],
      },
      'defines': [
        'USE_V4L2_CODEC'
      ],
      'dependencies': [
        '../media/media.gyp:media',
        '../third_party/libyuv/libyuv.gyp:libyuv',
      ],
      'sources': [
        'gpu/generic_v4l2_device.cc',
        'gpu/generic_v4l2_device.h',
        'gpu/v4l2_device.cc',
        'gpu/v4l2_device.h',
        'gpu/v4l2_image_processor.cc',
        'gpu/v4l2_image_processor.h',
        'gpu/v4l2_jpeg_decode_accelerator.cc',
        'gpu/v4l2_jpeg_decode_accelerator.h',
        'gpu/v4l2_slice_video_decode_accelerator.cc',
        'gpu/v4l2_slice_video_decode_accelerator.h',
        'gpu/v4l2_video_decode_accelerator.cc',
        'gpu/v4l2_video_decode_accelerator.h',
        'gpu/v4l2_video_encode_accelerator.cc',
        'gpu/v4l2_video_encode_accelerator.h',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/khronos',
      ],
    }],
    ['target_arch == "arm" and chromeos == 1', {
      'sources': [
        'gpu/tegra_v4l2_device.cc',
        'gpu/tegra_v4l2_device.h',
      ],
    }],
    ['target_arch != "arm" and chromeos == 1', {
      'dependencies': [
        '../media/media.gyp:media',
        '../third_party/libyuv/libyuv.gyp:libyuv',
      ],
      'sources': [
        'gpu/va_surface.h',
        'gpu/vaapi_jpeg_decode_accelerator.cc',
        'gpu/vaapi_jpeg_decode_accelerator.h',
        'gpu/vaapi_jpeg_decoder.cc',
        'gpu/vaapi_jpeg_decoder.h',
        'gpu/vaapi_picture.cc',
        'gpu/vaapi_picture.h',
        'gpu/vaapi_video_decode_accelerator.cc',
        'gpu/vaapi_video_decode_accelerator.h',
        'gpu/vaapi_video_encode_accelerator.cc',
        'gpu/vaapi_video_encode_accelerator.h',
        'gpu/vaapi_wrapper.cc',
        'gpu/vaapi_wrapper.h',
      ],
      'conditions': [
        ['use_x11 == 1', {
          'dependencies': [
            '../build/linux/system.gyp:x11',
            '../ui/gfx/x/gfx_x11.gyp:gfx_x11',
          ],
          'variables': {
            'sig_files': [
              'gpu/va.sigs',
              'gpu/va_x11.sigs',
            ],
          },
          'sources': [
            'gpu/vaapi_tfp_picture.cc',
            'gpu/vaapi_tfp_picture.h',
          ],
        }, {
          'variables': {
            'sig_files': [
              'gpu/va.sigs',
              'gpu/va_drm.sigs',
            ],
          },
          'sources': [
            'gpu/vaapi_drm_picture.cc',
            'gpu/vaapi_drm_picture.h',
          ],
        }],
      ],
      'variables': {
        'generate_stubs_script': '../tools/generate_stubs/generate_stubs.py',
        'extra_header': 'gpu/va_stub_header.fragment',
        'outfile_type': 'posix_stubs',
        'stubs_filename_root': 'va_stubs',
        'project_path': 'media/gpu',
        'intermediate_dir': '<(INTERMEDIATE_DIR)',
        'output_root': '<(SHARED_INTERMEDIATE_DIR)/va',
      },
      'include_dirs': [
        '<(DEPTH)/third_party/libva',
        '<(DEPTH)/third_party/libyuv',
        '<(output_root)',
      ],
      'actions': [
        {
          'action_name': 'generate_stubs',
          'inputs': [
            '<(generate_stubs_script)',
            '<(extra_header)',
            '<@(sig_files)',
          ],
          'outputs': [
            '<(intermediate_dir)/<(stubs_filename_root).cc',
            '<(output_root)/<(project_path)/<(stubs_filename_root).h',
          ],
          'action': ['python',
                     '<(generate_stubs_script)',
                     '-i', '<(intermediate_dir)',
                     '-o', '<(output_root)/<(project_path)',
                     '-t', '<(outfile_type)',
                     '-e', '<(extra_header)',
                     '-s', '<(stubs_filename_root)',
                     '-p', '<(project_path)',
                     '<@(_inputs)',
          ],
          'process_outputs_as_sources': 1,
          'message': 'Generating libva stubs for dynamic loading',
        },
     ]
    }],
    ['OS=="win"', {
      'dependencies': [
        '../media/media.gyp:media',
        '../media/media.gyp:mf_initializer',
        '../ui/gl/gl.gyp:gl',
        '../ui/gl/init/gl_init.gyp:gl_init',
      ],
      'link_settings': {
        'libraries': [
           '-ld3d9.lib',
           '-ld3d11.lib',
           '-ldxva2.lib',
           '-lstrmiids.lib',
           '-lmf.lib',
           '-lmfplat.lib',
           '-lmfuuid.lib',
        ],
        'msvs_settings': {
          'VCLinkerTool': {
            'DelayLoadDLLs': [
              'd3d9.dll',
              'd3d11.dll',
              'dxva2.dll',
              'mf.dll',
              'mfplat.dll',
            ],
          },
        },
      },
      'sources': [
        'gpu/dxva_picture_buffer_win.cc',
        'gpu/dxva_picture_buffer_win.h',
        'gpu/dxva_video_decode_accelerator_win.cc',
        'gpu/dxva_video_decode_accelerator_win.h',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/khronos',
      ],
    }],
    ['OS == "win" and target_arch == "x64"', {
      'msvs_settings': {
        'VCCLCompilerTool': {
          'AdditionalOptions': [
            '/wd4267', # Conversion from 'size_t' to 'type', possible loss of data
          ],
        },
      },
    }],
  ],
}
