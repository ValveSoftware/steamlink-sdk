# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //media/gpu/ipc/common
      'target_name': 'media_gpu_ipc_common',
      'type': 'static_library',
      'dependencies': [
        '../../media.gyp:media',
        '../../media.gyp:media_features',
        '../../../base/base.gyp:base',
        '../../../gpu/gpu.gyp:gpu_ipc_common',
        '../../../ipc/ipc.gyp:ipc',
        '../../../ui/gfx/gfx.gyp:gfx',
        '../../../ui/gfx/gfx.gyp:gfx_geometry',
        '../../../ui/gfx/ipc/geometry/gfx_ipc_geometry.gyp:gfx_ipc_geometry',
        '../../../ui/gfx/ipc/gfx_ipc.gyp:gfx_ipc',
      ],
      # This sources list is duplicated in //media/gpu/ipc/common/BUILD.gn
      'sources': [
        'common/create_video_encoder_params.cc',
        'common/create_video_encoder_params.h',
        'common/media_message_generator.cc',
        'common/media_message_generator.h',
        'common/media_messages.h',
        'common/media_param_traits.h',
        'common/media_param_traits.cc',
        'common/media_param_traits_macros.h',
      ],
    },
    {
      # GN version: //media/gpu/ipc/client
      'target_name': 'media_gpu_ipc_client',
      'type': 'static_library',
      'dependencies': [
        '../../media.gyp:media',
        '../../media.gyp:media_features',
        '../../media.gyp:media_gpu',
        '../../../base/base.gyp:base',
        '../../../gpu/gpu.gyp:gpu_ipc_common',
        '../../../ipc/ipc.gyp:ipc',
        '../../../ui/gfx/gfx.gyp:gfx',
        '../../../ui/gfx/gfx.gyp:gfx_geometry',
        '../../../ui/gfx/ipc/gfx_ipc.gyp:gfx_ipc',
      ],
      # This sources list is duplicated in //media/gpu/ipc/client/BUILD.gn
      'sources': [
        'client/gpu_jpeg_decode_accelerator_host.cc',
        'client/gpu_jpeg_decode_accelerator_host.h',
        'client/gpu_video_decode_accelerator_host.cc',
        'client/gpu_video_decode_accelerator_host.h',
        'client/gpu_video_encode_accelerator_host.cc',
        'client/gpu_video_encode_accelerator_host.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267 ],
    },
    {
      # GN version: //media/gpu/ipc/service
      'target_name': 'media_gpu_ipc_service',
      'type': 'static_library',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../gpu/gpu.gyp:gpu_config',
        '../../../gpu/gpu.gyp:gpu_ipc_service',
        '../../../ipc/ipc.gyp:ipc',
        '../../../third_party/khronos/khronos.gyp:khronos_headers',
        '../../../third_party/mesa/mesa.gyp:mesa_headers',
        '../../media.gyp:media',
        '../../media.gyp:media_gpu',
        'media_gpu_ipc_common',
      ],
      'sources': [
        'service/gpu_jpeg_decode_accelerator.cc',
        'service/gpu_jpeg_decode_accelerator.h',
        'service/gpu_video_decode_accelerator.cc',
        'service/gpu_video_decode_accelerator.h',
        'service/gpu_video_encode_accelerator.cc',
        'service/gpu_video_encode_accelerator.h',
        'service/media_channel.cc',
        'service/media_channel.h',
        'service/media_service.cc',
        'service/media_service.h',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/libva',
        '<(DEPTH)/third_party/mesa/src/include',
      ],
      'conditions': [
        ['OS == "win" and target_arch == "x64"', {
          'msvs_settings': {
            'VCCLCompilerTool': {
              'AdditionalOptions': [
                '/wd4267', # Conversion from 'size_t' to 'type', possible loss of data
              ],
            },
          },
        }],
        ['OS=="mac"', {
          'dependencies': [
            '../../../third_party/webrtc/common_video/common_video.gyp:common_video',
          ],
        }],
      ],
    }
  ]
}
