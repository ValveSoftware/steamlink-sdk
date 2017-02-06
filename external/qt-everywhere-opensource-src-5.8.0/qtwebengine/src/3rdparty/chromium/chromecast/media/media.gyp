# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'chromecast_branding%': 'public',
    'use_alsa%': 0,

    # Set true if the ALSA library being used supports raw timestamps
    'use_alsa_monotonic_raw_tstamps%': 0,
  },
  'target_defaults': {
    'include_dirs': [
      '../..',  # Root of Chromium checkout
      '../public/',  # Public APIs
    ],
    'target_conditions': [
      ['_type=="executable"', {
        'ldflags': [
          # Allow  OEMs to override default libraries that are shipped with
          # cast receiver package by installed OEM-specific libraries in
          # /oem_cast_shlib.
          '-Wl,-rpath=/oem_cast_shlib',
          # Some shlibs are built in same directory of executables.
          '-Wl,-rpath=\$$ORIGIN',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'media_audio',
      'type': '<(component)',
      'dependencies': [
        '../../media/media.gyp:media',
        'cma_backend_manager',
      ],
      'sources': [
        'audio/cast_audio_manager.cc',
        'audio/cast_audio_manager.h',
        'audio/cast_audio_manager_factory.cc',
        'audio/cast_audio_manager_factory.h',
        'audio/cast_audio_output_stream.cc',
        'audio/cast_audio_output_stream.h',
      ],
    },
    {
      'target_name': 'media_base',
      'type': '<(component)',
      'dependencies': [
        'libcast_media_1.0',
        '../../base/base.gyp:base',
        '../../crypto/crypto.gyp:crypto',
        '../../third_party/widevine/cdm/widevine_cdm.gyp:widevine_cdm_version_h',
      ],
      'sources': [
        'base/decrypt_context_impl.cc',
        'base/decrypt_context_impl.h',
        'base/decrypt_context_impl_clearkey.cc',
        'base/decrypt_context_impl_clearkey.h',
        'base/key_systems_common.cc',
        'base/key_systems_common.h',
        'base/media_caps.cc',
        'base/media_caps.h',
        'base/media_codec_support.cc',
        'base/media_codec_support.h',
        'base/media_message_loop.cc',
        'base/media_message_loop.h',
        'base/media_resource_tracker.cc',
        'base/media_resource_tracker.h',
        'base/video_plane_controller.cc',
        'base/video_plane_controller.h',
      ],
    },
    {
      'target_name': 'media_cdm',
      'type': '<(component)',
      'dependencies': [
        'media_base',
        '../../base/base.gyp:base',
        '../../media/media.gyp:media',
      ],
      'sources': [
        'cdm/browser_cdm_cast.cc',
        'cdm/browser_cdm_cast.h',
        'cdm/chromecast_init_data.cc',
        'cdm/chromecast_init_data.h',
      ],
    },
    {
      'target_name': 'cma_base',
      'type': '<(component)',
      'dependencies': [
        '../chromecast.gyp:cast_base',
        '../../base/base.gyp:base',
        '../../media/media.gyp:media',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'cma/base/balanced_media_task_runner_factory.cc',
        'cma/base/balanced_media_task_runner_factory.h',
        'cma/base/buffering_controller.cc',
        'cma/base/buffering_controller.h',
        'cma/base/buffering_defs.cc',
        'cma/base/buffering_defs.h',
        'cma/base/buffering_frame_provider.cc',
        'cma/base/buffering_frame_provider.h',
        'cma/base/buffering_state.cc',
        'cma/base/buffering_state.h',
        'cma/base/cast_decrypt_config_impl.cc',
        'cma/base/cast_decrypt_config_impl.h',
        'cma/base/cma_logging.h',
        'cma/base/coded_frame_provider.cc',
        'cma/base/coded_frame_provider.h',
        'cma/base/decoder_buffer_adapter.cc',
        'cma/base/decoder_buffer_adapter.h',
        'cma/base/decoder_config_adapter.cc',
        'cma/base/decoder_config_adapter.h',
        'cma/base/demuxer_stream_adapter.cc',
        'cma/base/demuxer_stream_adapter.h',
        'cma/base/media_task_runner.cc',
        'cma/base/media_task_runner.h',
        'cma/base/simple_media_task_runner.cc',
        'cma/base/simple_media_task_runner.h',
      ],
    },
    {
      'target_name': 'default_cma_backend',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'cma/backend/audio_decoder_default.cc',
        'cma/backend/audio_decoder_default.h',
        'cma/backend/media_pipeline_backend_default.cc',
        'cma/backend/media_pipeline_backend_default.h',
        'cma/backend/video_decoder_default.cc',
        'cma/backend/video_decoder_default.h',
      ],
    },
    {
      'target_name': 'cma_backend_manager',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'cma/backend/audio_decoder_wrapper.cc',
        'cma/backend/audio_decoder_wrapper.h',
        'cma/backend/media_pipeline_backend_manager.cc',
        'cma/backend/media_pipeline_backend_manager.h',
        'cma/backend/media_pipeline_backend_wrapper.cc',
        'cma/backend/media_pipeline_backend_wrapper.h',
      ],
    },
    {
      'target_name': 'cma_decoder',
      'type': '<(component)',
      'dependencies': [
        'cma_base',
        '../../base/base.gyp:base',
        '../../media/media.gyp:media',
      ],
      'sources': [
        'cma/decoder/cast_audio_decoder.h',
        'cma/decoder/cast_audio_decoder_android.cc',
        'cma/decoder/cast_audio_decoder_linux.cc',
      ],
    },
    {
      'target_name': 'cma_ipc',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
      ],
      'sources': [
        'cma/ipc/media_memory_chunk.cc',
        'cma/ipc/media_memory_chunk.h',
        'cma/ipc/media_message.cc',
        'cma/ipc/media_message.h',
        'cma/ipc/media_message_fifo.cc',
        'cma/ipc/media_message_fifo.h',
      ],
    },
    {
      'target_name': 'cma_ipc_streamer',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../media/media.gyp:media',
        'cma_base',
      ],
      'sources': [
        'cma/ipc_streamer/audio_decoder_config_marshaller.cc',
        'cma/ipc_streamer/audio_decoder_config_marshaller.h',
        'cma/ipc_streamer/av_streamer_proxy.cc',
        'cma/ipc_streamer/av_streamer_proxy.h',
        'cma/ipc_streamer/coded_frame_provider_host.cc',
        'cma/ipc_streamer/coded_frame_provider_host.h',
        'cma/ipc_streamer/decoder_buffer_base_marshaller.cc',
        'cma/ipc_streamer/decoder_buffer_base_marshaller.h',
        'cma/ipc_streamer/decrypt_config_marshaller.cc',
        'cma/ipc_streamer/decrypt_config_marshaller.h',
        'cma/ipc_streamer/encryption_scheme_marshaller.cc',
        'cma/ipc_streamer/encryption_scheme_marshaller.h',
        'cma/ipc_streamer/video_decoder_config_marshaller.cc',
        'cma/ipc_streamer/video_decoder_config_marshaller.h',
      ],
    },
    {
      'target_name': 'cma_pipeline',
      'type': '<(component)',
      'dependencies': [
        'cma_base',
        'cma_decoder',
        'media_base',
        'media_cdm',
        '../../base/base.gyp:base',
        '../../crypto/crypto.gyp:crypto',
        '../../media/media.gyp:media',
        '../../third_party/boringssl/boringssl.gyp:boringssl',
      ],
      'sources': [
        'cma/pipeline/audio_decoder_software_wrapper.cc',
        'cma/pipeline/audio_decoder_software_wrapper.h',
        'cma/pipeline/audio_pipeline_impl.cc',
        'cma/pipeline/audio_pipeline_impl.h',
        'cma/pipeline/av_pipeline_client.cc',
        'cma/pipeline/av_pipeline_client.h',
        'cma/pipeline/av_pipeline_impl.cc',
        'cma/pipeline/av_pipeline_impl.h',
        'cma/pipeline/decrypt_util.cc',
        'cma/pipeline/decrypt_util.h',
        'cma/pipeline/load_type.h',
        'cma/pipeline/media_pipeline_client.cc',
        'cma/pipeline/media_pipeline_client.h',
        'cma/pipeline/media_pipeline_impl.cc',
        'cma/pipeline/media_pipeline_impl.h',
        'cma/pipeline/video_pipeline_client.cc',
        'cma/pipeline/video_pipeline_client.h',
        'cma/pipeline/video_pipeline_impl.cc',
        'cma/pipeline/video_pipeline_impl.h',
      ],
    },
    {
      'target_name': 'cast_media',
      'type': 'none',
      'dependencies': [
        'cma_base',
        'cma_ipc',
        'cma_ipc_streamer',
        'cma_pipeline',
        'default_cma_backend',
        'media_audio',
        'media_cdm',
      ],
    },
    {
      'target_name': 'cast_media_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'cast_media',
        '../../base/base.gyp:base',
        '../../base/base.gyp:base_i18n',
        '../../base/base.gyp:test_support_base',
        '../../chromecast/chromecast.gyp:cast_metrics_test_support',
        '../../gpu/gpu.gyp:gpu_unittest_utils',
        '../../media/media.gyp:media_test_support',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        '../../testing/gtest.gyp:gtest_main',
        '../../ui/gfx/gfx.gyp:gfx_test_support',
      ],
      'sources': [
        'audio/cast_audio_output_stream_unittest.cc',
        'base/media_resource_tracker_unittest.cc',
        'cdm/chromecast_init_data_unittest.cc',
        'cma/backend/audio_video_pipeline_device_unittest.cc',
        'cma/base/balanced_media_task_runner_unittest.cc',
        'cma/base/buffering_controller_unittest.cc',
        'cma/base/buffering_frame_provider_unittest.cc',
        'cma/base/demuxer_stream_adapter_unittest.cc',
        'cma/base/demuxer_stream_for_test.cc',
        'cma/base/demuxer_stream_for_test.h',
        'cma/base/multi_demuxer_stream_adapter_unittest.cc',
        'cma/ipc/media_message_fifo_unittest.cc',
        'cma/ipc/media_message_unittest.cc',
        'cma/ipc_streamer/av_streamer_unittest.cc',
        'cma/pipeline/audio_video_pipeline_impl_unittest.cc',
        'cma/test/frame_generator_for_test.cc',
        'cma/test/frame_generator_for_test.h',
        'cma/test/frame_segmenter_for_test.cc',
        'cma/test/frame_segmenter_for_test.h',
        'cma/test/mock_frame_consumer.cc',
        'cma/test/mock_frame_consumer.h',
        'cma/test/mock_frame_provider.cc',
        'cma/test/mock_frame_provider.h',
        'cma/test/run_all_unittests.cc',
      ],
      'conditions': [
        ['chromecast_branding=="public"', {
          'dependencies': [
            # Link default libcast_media_1.0 statically not to link dummy one
            # dynamically for public unittests.
            'libcast_media_1.0_default_core',
          ],
        }],
      ],
    },
    {
      'target_name': 'cast_multizone_backend_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'cast_media',
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../chromecast/chromecast.gyp:cast_metrics_test_support',
        '../../media/media.gyp:media_test_support',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        '../../testing/gtest.gyp:gtest_main',
      ],
      'sources': [
        'cma/backend/multizone_backend_unittest.cc',
        'cma/test/run_all_unittests.cc',
      ],
      'conditions': [
        ['chromecast_branding=="public"', {
          'dependencies': [
            # Link default libcast_media_1.0 statically not to link dummy one
            # dynamically for public unittests.
            'libcast_media_1.0_default_core',
          ],
        }],
      ],
    },
    { # Target for OEM partners to override media shared library, i.e.
      # libcast_media_1.0.so. This target is only used to build executables
      # with correct linkage information.
      'target_name': 'libcast_media_1.0',
      'type': 'shared_library',
      'dependencies': [
        '../../chromecast/chromecast.gyp:cast_public_api',
      ],
      'sources': [
        'base/cast_media_dummy.cc',
      ],
    },
    { # This target can be statically linked into unittests, but production
      # binaries should not depend on this target.
      'target_name': 'libcast_media_1.0_default_core',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/chromecast/chromecast.gyp:cast_base',
        '../../chromecast/chromecast.gyp:cast_public_api',
        'default_cma_backend'
      ],
      'sources': [
        'base/cast_media_default.cc',
      ],
    },
    { # Default implementation of libcast_media_1.0.so.
      'target_name': 'libcast_media_1.0_default',
      'type': 'loadable_module',
      # Cannot depend on libcast_media_1.0_default_core since a loadable_module
      # include only symbols necessary for source files. So, it should include
      # top-level .cc, here cast_media_default.cc explicitly.
      'dependencies': [
        '../../chromecast/chromecast.gyp:cast_public_api',
        'default_cma_backend'
      ],
      'sources': [
        'base/cast_media_default.cc',
      ],
    },
    { # Alsa implementation of CMA backend.
      'target_name': 'alsa_cma_backend',
      'type': '<(component)',
      'dependencies': [
        'chromecast_alsa_features',
        'cma_base',
        'cma_decoder',
        'default_cma_backend',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/chromecast/chromecast.gyp:cast_base',
        '<(DEPTH)/chromecast/chromecast.gyp:cast_public_api',
        '<(DEPTH)/media/media.gyp:media',
      ],
      'sources': [
        'cma/backend/alsa/alsa_wrapper.cc',
        'cma/backend/alsa/alsa_wrapper.h',
        'cma/backend/alsa/audio_decoder_alsa.cc',
        'cma/backend/alsa/audio_decoder_alsa.h',
        'cma/backend/alsa/media_pipeline_backend_alsa.cc',
        'cma/backend/alsa/media_pipeline_backend_alsa.h',
        'cma/backend/alsa/stream_mixer_alsa.cc',
        'cma/backend/alsa/stream_mixer_alsa.h',
        'cma/backend/alsa/stream_mixer_alsa_input.cc',
        'cma/backend/alsa/stream_mixer_alsa_input.h',
        'cma/backend/alsa/stream_mixer_alsa_input_impl.cc',
        'cma/backend/alsa/stream_mixer_alsa_input_impl.h',
      ],
    },  # end of target 'alsa_cma_backend'
    {
      # GN target: //chromecast/media/cma/backend/alsa:alsa_features
      'target_name': 'chromecast_alsa_features',
      'includes': [ '../../build/buildflag_header.gypi' ],
      'variables': {
        'buildflag_header_path': 'chromecast/media/cma/backend/alsa/alsa_features.h',
        'buildflag_flags': [
          'ALSA_MONOTONIC_RAW_TSTAMPS=<(use_alsa_monotonic_raw_tstamps)',
        ]
      }
    },  # end of target 'alsa_features'
    { # Alsa implementation of libcast_media_1.0.
      'target_name': 'libcast_media_1.0_audio',
      'type': 'loadable_module',
      'dependencies': [
        'alsa_cma_backend',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/chromecast/chromecast.gyp:cast_base',
        '<(DEPTH)/chromecast/chromecast.gyp:cast_public_api',
        '<(DEPTH)/chromecast/media/media.gyp:default_cma_backend',
      ],
      'sources': [
        'cma/backend/alsa/cast_media_shlib.cc',
        'cma/backend/alsa/media_codec_support_cast_audio.cc',
        # NOTE: can't depend on media_base because that pulls in libcast_media.
        '<(DEPTH)/chromecast/media/base/media_caps.cc',
        '<(DEPTH)/chromecast/media/base/media_caps.h',
      ],
    },  # end of target 'libcast_media_1.0_audio'
  ],  # end of targets
  'conditions': [
    ['use_alsa==1', {
      'targets': [
        {
          'target_name': 'cast_alsa_cma_backend_unittests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            'alsa_cma_backend',
            '<(DEPTH)/base/base.gyp:run_all_unittests',
            '<(DEPTH)/testing/gmock.gyp:gmock',
            '<(DEPTH)/testing/gtest.gyp:gtest',
           ],
          'sources': [
            'cma/backend/alsa/mock_alsa_wrapper.cc',
            'cma/backend/alsa/mock_alsa_wrapper.h',
            'cma/backend/alsa/stream_mixer_alsa_unittest.cc',
          ],
        },  # end of target 'cast_alsa_cma_backend_unittests'
      ],  # end of targets
    }],
  ],  # end of conditions
}
