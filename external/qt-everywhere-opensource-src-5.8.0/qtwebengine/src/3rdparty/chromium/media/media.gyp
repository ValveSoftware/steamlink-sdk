# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    # Override to dynamically link the cras (ChromeOS audio) library.
    'use_cras%': 0,
    # Option e.g. for Linux distributions to link pulseaudio directly
    # (DT_NEEDED) instead of using dlopen. This helps with automated
    # detection of ABI mismatches and prevents silent errors.
    'linux_link_pulseaudio%': 0,
    'conditions': [
      # Enable ALSA and Pulse for runtime selection.
      ['(OS=="linux" or OS=="freebsd" or OS=="solaris") and (embedded==0 or chromecast==1)', {
        # ALSA is always needed for Web MIDI even if the cras is enabled.
        'use_alsa%': 1,
        'conditions': [
          ['use_cras==1 or chromecast==1', {
            'use_pulseaudio%': 0,
          }, {
            'use_pulseaudio%': 1,
          }],
        ],
      }, {
        'use_alsa%': 0,
        'use_pulseaudio%': 0,
      }],
      # low memory buffer is used in non-Android based chromecast build due to hardware limitation.
      ['chromecast==1 and OS!="android"', {
        'use_low_memory_buffer%': 1,
      }, {
        'use_low_memory_buffer%': 0,
      }],
      ['proprietary_codecs==1 and chromecast==1', {
        # Enable AC3/EAC3 audio demuxing. Actual decoding must be provided by
        # the platform (or HDMI sink in Chromecast for audio pass-through case).
        'enable_ac3_eac3_audio_demuxing%': 1,
        # Enable HEVC/H265 demuxing. Actual decoding must be provided by the
        # platform.
        'enable_hevc_demuxing%': 1,
        'enable_mse_mpeg2ts_stream_parser%': 1,
      }, {
        'enable_ac3_eac3_audio_demuxing%': 0,
        'enable_hevc_demuxing%': 0,
        'enable_mse_mpeg2ts_stream_parser%': 0,
      }],
    ],
  },
  'includes': [
    'media_cdm.gypi',
    'media_variables.gypi',
  ],
  'targets': [
    {
      # GN version: //media:media_features
      'target_name': 'media_features',
      'includes': [ '../build/buildflag_header.gypi' ],
      'hard_dependency': 1,
      'variables': {
        'buildflag_header_path': 'media/media_features.h',
        'buildflag_flags': [
          "ENABLE_AC3_EAC3_AUDIO_DEMUXING=<(enable_ac3_eac3_audio_demuxing)",
          "ENABLE_HEVC_DEMUXING=<(enable_hevc_demuxing)",
          "ENABLE_MSE_MPEG2TS_STREAM_PARSER=<(enable_mse_mpeg2ts_stream_parser)",
        ],
      },
    },
    {
      # GN version: //media
      'target_name': 'media',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/third_party/khronos/khronos.gyp:khronos_headers',
        'media_features',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../crypto/crypto.gyp:crypto',
        '../gpu/gpu.gyp:command_buffer_common',
        '../skia/skia.gyp:skia',
        '../third_party/libwebm/libwebm.gyp:libwebm',
        '../third_party/libyuv/libyuv.gyp:libyuv',
        '../third_party/opus/opus.gyp:opus',
        '../ui/display/display.gyp:display',
        '../ui/events/events.gyp:events_base',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../url/url.gyp:url_lib',
        'shared_memory_support',
      ],
      'hard_dependency': 1,
      'export_dependent_settings': [
        '../third_party/libwebm/libwebm.gyp:libwebm',
        '../third_party/opus/opus.gyp:opus',
        'media_features',
      ],
      'defines': [
        'MEDIA_IMPLEMENTATION',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'audio/agc_audio_stream.h',
        'audio/alsa/alsa_input.cc',
        'audio/alsa/alsa_input.h',
        'audio/alsa/alsa_output.cc',
        'audio/alsa/alsa_output.h',
        'audio/alsa/alsa_util.cc',
        'audio/alsa/alsa_util.h',
        'audio/alsa/alsa_wrapper.cc',
        'audio/alsa/alsa_wrapper.h',
        'audio/alsa/audio_manager_alsa.cc',
        'audio/alsa/audio_manager_alsa.h',
        'audio/android/audio_manager_android.cc',
        'audio/android/audio_manager_android.h',
        'audio/android/audio_record_input.cc',
        'audio/android/audio_record_input.h',
        'audio/android/opensles_input.cc',
        'audio/android/opensles_input.h',
        'audio/android/opensles_output.cc',
        'audio/android/opensles_output.h',
        'audio/android/opensles_util.cc',
        'audio/android/opensles_util.h',
        'audio/android/opensles_wrapper.cc',
        'audio/audio_device_description.cc',
        'audio/audio_device_description.h',
        'audio/audio_device_name.cc',
        'audio/audio_device_name.h',
        'audio/audio_device_thread.cc',
        'audio/audio_device_thread.h',
        'audio/audio_input_controller.cc',
        'audio/audio_input_controller.h',
        'audio/audio_input_device.cc',
        'audio/audio_input_device.h',
        'audio/audio_input_ipc.cc',
        'audio/audio_input_ipc.h',
        'audio/audio_io.h',
        'audio/audio_manager.cc',
        'audio/audio_manager.h',
        'audio/audio_manager_base.cc',
        'audio/audio_manager_base.h',
        'audio/audio_output_controller.cc',
        'audio/audio_output_controller.h',
        'audio/audio_output_device.cc',
        'audio/audio_output_device.h',
        'audio/audio_output_dispatcher.cc',
        'audio/audio_output_dispatcher.h',
        'audio/audio_output_dispatcher_impl.cc',
        'audio/audio_output_dispatcher_impl.h',
        'audio/audio_output_ipc.cc',
        'audio/audio_output_ipc.h',
        'audio/audio_output_proxy.cc',
        'audio/audio_output_proxy.h',
        'audio/audio_output_resampler.cc',
        'audio/audio_output_resampler.h',
        'audio/audio_output_stream_sink.cc',
        'audio/audio_output_stream_sink.h',
        'audio/audio_power_monitor.cc',
        'audio/audio_power_monitor.h',
        'audio/audio_source_diverter.h',
        'audio/audio_streams_tracker.cc',
        'audio/audio_streams_tracker.h',
        'audio/clockless_audio_sink.cc',
        'audio/clockless_audio_sink.h',
        'audio/cras/audio_manager_cras.cc',
        'audio/cras/audio_manager_cras.h',
        'audio/cras/cras_input.cc',
        'audio/cras/cras_input.h',
        'audio/cras/cras_unified.cc',
        'audio/cras/cras_unified.h',
        'audio/fake_audio_input_stream.cc',
        'audio/fake_audio_input_stream.h',
        'audio/fake_audio_log_factory.cc',
        'audio/fake_audio_log_factory.h',
        'audio/fake_audio_manager.cc',
        'audio/fake_audio_manager.h',
        'audio/fake_audio_output_stream.cc',
        'audio/fake_audio_output_stream.h',
        'audio/fake_audio_worker.cc',
        'audio/fake_audio_worker.h',
        'audio/linux/audio_manager_linux.cc',
        'audio/mac/audio_auhal_mac.cc',
        'audio/mac/audio_auhal_mac.h',
        'audio/mac/audio_device_listener_mac.cc',
        'audio/mac/audio_device_listener_mac.h',
        'audio/mac/audio_input_mac.cc',
        'audio/mac/audio_input_mac.h',
        'audio/mac/audio_low_latency_input_mac.cc',
        'audio/mac/audio_low_latency_input_mac.h',
        'audio/mac/audio_manager_mac.cc',
        'audio/mac/audio_manager_mac.h',
        'audio/null_audio_sink.cc',
        'audio/null_audio_sink.h',
        'audio/pulse/audio_manager_pulse.cc',
        'audio/pulse/audio_manager_pulse.h',
        'audio/pulse/pulse_input.cc',
        'audio/pulse/pulse_input.h',
        'audio/pulse/pulse_output.cc',
        'audio/pulse/pulse_output.h',
        'audio/pulse/pulse_util.cc',
        'audio/pulse/pulse_util.h',
        'audio/sample_rates.cc',
        'audio/sample_rates.h',
        'audio/scoped_task_runner_observer.cc',
        'audio/scoped_task_runner_observer.h',
        'audio/simple_sources.cc',
        'audio/simple_sources.h',
        'audio/sounds/audio_stream_handler.cc',
        'audio/sounds/audio_stream_handler.h',
        'audio/sounds/sounds_manager.cc',
        'audio/sounds/sounds_manager.h',
        'audio/sounds/wav_audio_handler.cc',
        'audio/sounds/wav_audio_handler.h',
        'audio/virtual_audio_input_stream.cc',
        'audio/virtual_audio_input_stream.h',
        'audio/virtual_audio_output_stream.cc',
        'audio/virtual_audio_output_stream.h',
        'audio/virtual_audio_sink.cc',
        'audio/virtual_audio_sink.h',
        'audio/win/audio_device_listener_win.cc',
        'audio/win/audio_device_listener_win.h',
        'audio/win/audio_low_latency_input_win.cc',
        'audio/win/audio_low_latency_input_win.h',
        'audio/win/audio_low_latency_output_win.cc',
        'audio/win/audio_low_latency_output_win.h',
        'audio/win/audio_manager_win.cc',
        'audio/win/audio_manager_win.h',
        'audio/win/avrt_wrapper_win.cc',
        'audio/win/avrt_wrapper_win.h',
        'audio/win/core_audio_util_win.cc',
        'audio/win/core_audio_util_win.h',
        'audio/win/device_enumeration_win.cc',
        'audio/win/device_enumeration_win.h',
        'audio/win/wavein_input_win.cc',
        'audio/win/wavein_input_win.h',
        'audio/win/waveout_output_win.cc',
        'audio/win/waveout_output_win.h',
        'base/audio_block_fifo.cc',
        'base/audio_block_fifo.h',
        'base/audio_buffer.cc',
        'base/audio_buffer.h',
        'base/audio_buffer_converter.cc',
        'base/audio_buffer_converter.h',
        'base/audio_buffer_queue.cc',
        'base/audio_buffer_queue.h',
        'base/audio_capturer_source.h',
        'base/audio_codecs.cc',
        'base/audio_codecs.h',
        'base/audio_converter.cc',
        'base/audio_converter.h',
        'base/audio_decoder.cc',
        'base/audio_decoder.h',
        'base/audio_decoder_config.cc',
        'base/audio_decoder_config.h',
        'base/audio_discard_helper.cc',
        'base/audio_discard_helper.h',
        'base/audio_fifo.cc',
        'base/audio_fifo.h',
        'base/audio_hardware_config.cc',
        'base/audio_hardware_config.h',
        'base/audio_hash.cc',
        'base/audio_hash.h',
        'base/audio_latency.cc',
        'base/audio_latency.h',
        'base/audio_pull_fifo.cc',
        'base/audio_pull_fifo.h',
        'base/audio_push_fifo.cc',
        'base/audio_push_fifo.h',
        'base/audio_renderer.cc',
        'base/audio_renderer.h',
        'base/audio_renderer_mixer.cc',
        'base/audio_renderer_mixer.h',
        'base/audio_renderer_mixer_input.cc',
        'base/audio_renderer_mixer_input.h',
        'base/audio_renderer_mixer_pool.h',
        'base/audio_renderer_sink.h',
        'base/audio_shifter.cc',
        'base/audio_shifter.h',
        'base/audio_splicer.cc',
        'base/audio_splicer.h',
        'base/audio_timestamp_helper.cc',
        'base/audio_timestamp_helper.h',
        'base/audio_video_metadata_extractor.cc',
        'base/audio_video_metadata_extractor.h',
        'base/bind_to_current_loop.h',
        'base/bit_reader.cc',
        'base/bit_reader.h',
        'base/bit_reader_core.cc',
        'base/bit_reader_core.h',
        'base/bitstream_buffer.cc',
        'base/bitstream_buffer.h',
        'base/buffering_state.h',
        'base/byte_queue.cc',
        'base/byte_queue.h',
        'base/cdm_callback_promise.cc',
        'base/cdm_callback_promise.h',
        'base/cdm_config.h',
        'base/cdm_context.cc',
        'base/cdm_context.h',
        'base/cdm_factory.cc',
        'base/cdm_factory.h',
        'base/cdm_initialized_promise.cc',
        'base/cdm_initialized_promise.h',
        'base/cdm_key_information.cc',
        'base/cdm_key_information.h',
        'base/cdm_promise.cc',
        'base/cdm_promise.h',
        'base/cdm_promise_adapter.cc',
        'base/cdm_promise_adapter.h',
        'base/channel_mixer.cc',
        'base/channel_mixer.h',
        'base/channel_mixing_matrix.cc',
        'base/channel_mixing_matrix.h',
        'base/container_names.cc',
        'base/container_names.h',
        'base/data_buffer.cc',
        'base/data_buffer.h',
        'base/data_source.cc',
        'base/data_source.h',
        'base/decode_status.cc',
        'base/decode_status.h',
        'base/decoder_buffer.cc',
        'base/decoder_buffer.h',
        'base/decoder_buffer_queue.cc',
        'base/decoder_buffer_queue.h',
        'base/decoder_factory.cc',
        'base/decoder_factory.h',
        'base/decrypt_config.cc',
        'base/decrypt_config.h',
        'base/decryptor.cc',
        'base/decryptor.h',
        'base/demuxer.cc',
        'base/demuxer.h',
        'base/demuxer_stream.cc',
        'base/demuxer_stream.h',
        'base/demuxer_stream_provider.cc',
        'base/demuxer_stream_provider.h',
        'base/djb2.cc',
        'base/djb2.h',
        'base/eme_constants.h',
        'base/encryption_scheme.cc',
        'base/encryption_scheme.h',
        'base/key_system_names.cc',
        'base/key_system_names.h',
        'base/key_system_properties.cc',
        'base/key_system_properties.h',
        'base/key_systems.cc',
        'base/key_systems.h',
        'base/keyboard_event_counter.cc',
        'base/keyboard_event_counter.h',
        'base/loopback_audio_converter.cc',
        'base/loopback_audio_converter.h',
        'base/mac/avfoundation_glue.h',
        'base/mac/avfoundation_glue.mm',
        'base/mac/coremedia_glue.h',
        'base/mac/coremedia_glue.mm',
        'base/mac/corevideo_glue.h',
        'base/mac/video_frame_mac.cc',
        'base/mac/video_frame_mac.h',
        'base/mac/videotoolbox_glue.h',
        'base/mac/videotoolbox_glue.mm',
        'base/mac/videotoolbox_helpers.cc',
        'base/mac/videotoolbox_helpers.h',
        'base/media.cc',
        'base/media.h',
        'base/media_client.cc',
        'base/media_client.h',
        'base/media_file_checker.cc',
        'base/media_file_checker.h',
        'base/media_keys.cc',
        'base/media_keys.h',
        'base/media_log.cc',
        'base/media_log.h',
        'base/media_log_event.h',
        'base/media_permission.cc',
        'base/media_permission.h',
        'base/media_resources.cc',
        'base/media_resources.h',
        'base/media_switches.cc',
        'base/media_switches.h',
        'base/media_track.cc',
        'base/media_track.h',
        'base/media_tracks.cc',
        'base/media_tracks.h',
        'base/media_util.cc',
        'base/media_util.h',
        'base/mime_util.cc',
        'base/mime_util.h',
        'base/mime_util_internal.cc',
        'base/mime_util_internal.h',
        'base/moving_average.cc',
        'base/moving_average.h',
        'base/multi_channel_resampler.cc',
        'base/multi_channel_resampler.h',
        'base/null_video_sink.cc',
        'base/null_video_sink.h',
        'base/output_device_info.cc',
        'base/output_device_info.h',
        'base/pipeline.h',
        'base/pipeline_impl.cc',
        'base/pipeline_impl.h',
        'base/pipeline_metadata.h',
        'base/pipeline_status.h',
        'base/player_tracker.cc',
        'base/player_tracker.h',
        'base/ranges.cc',
        'base/ranges.h',
        'base/renderer.cc',
        'base/renderer.h',
        'base/renderer_factory.cc',
        'base/renderer_factory.h',
        'base/sample_format.cc',
        'base/sample_format.h',
        'base/seekable_buffer.cc',
        'base/seekable_buffer.h',
        'base/serial_runner.cc',
        'base/serial_runner.h',
        'base/simd/convert_rgb_to_yuv.h',
        'base/simd/convert_rgb_to_yuv_c.cc',
        'base/simd/convert_yuv_to_rgb.h',
        'base/simd/convert_yuv_to_rgb_c.cc',
        'base/simd/filter_yuv.h',
        'base/simd/filter_yuv_c.cc',
        'base/sinc_resampler.cc',
        'base/sinc_resampler.h',
        'base/stream_parser.cc',
        'base/stream_parser.h',
        'base/stream_parser_buffer.cc',
        'base/stream_parser_buffer.h',
        'base/text_cue.cc',
        'base/text_cue.h',
        'base/text_ranges.cc',
        'base/text_ranges.h',
        'base/text_renderer.cc',
        'base/text_renderer.h',
        'base/text_track.h',
        'base/text_track_config.cc',
        'base/text_track_config.h',
        'base/time_delta_interpolator.cc',
        'base/time_delta_interpolator.h',
        'base/time_source.h',
        'base/timestamp_constants.h',
        'base/user_input_monitor.cc',
        'base/user_input_monitor.h',
        'base/user_input_monitor_linux.cc',
        'base/user_input_monitor_mac.cc',
        'base/user_input_monitor_win.cc',
        'base/video_capture_types.cc',
        'base/video_capture_types.h',
        'base/video_capturer_source.cc',
        'base/video_capturer_source.h',
        'base/video_codecs.cc',
        'base/video_codecs.h',
        'base/video_decoder.cc',
        'base/video_decoder.h',
        'base/video_decoder_config.cc',
        'base/video_decoder_config.h',
        'base/video_frame.cc',
        'base/video_frame.h',
        'base/video_frame_metadata.cc',
        'base/video_frame_metadata.h',
        'base/video_frame_pool.cc',
        'base/video_frame_pool.h',
        'base/video_renderer.cc',
        'base/video_renderer.h',
        'base/video_rotation.h',
        'base/video_types.cc',
        'base/video_types.h',
        'base/video_util.cc',
        'base/video_util.h',
        'base/wall_clock_time_source.cc',
        'base/wall_clock_time_source.h',
        'base/yuv_convert.cc',
        'base/yuv_convert.h',
        'cdm/aes_decryptor.cc',
        'cdm/aes_decryptor.h',
        'cdm/cdm_adapter.cc',
        'cdm/cdm_adapter.h',
        'cdm/cdm_allocator.cc',
        'cdm/cdm_allocator.h',
        'cdm/cdm_file_io.h',
        'cdm/cdm_file_io.h',
        'cdm/cdm_helpers.cc',
        'cdm/cdm_helpers.h',
        'cdm/default_cdm_factory.cc',
        'cdm/default_cdm_factory.h',
        'cdm/json_web_key.cc',
        'cdm/json_web_key.h',
        'cdm/player_tracker_impl.cc',
        'cdm/player_tracker_impl.h',
        'cdm/supported_cdm_versions.cc',
        'cdm/supported_cdm_versions.h',
        'ffmpeg/ffmpeg_common.cc',
        'ffmpeg/ffmpeg_common.h',
        'ffmpeg/ffmpeg_deleters.h',
        'filters/audio_clock.cc',
        'filters/audio_clock.h',
        'filters/audio_file_reader.cc',
        'filters/audio_file_reader.h',
        'filters/audio_renderer_algorithm.cc',
        'filters/audio_renderer_algorithm.h',
        'filters/audio_timestamp_validator.cc',
        'filters/audio_timestamp_validator.h',
        'filters/blocking_url_protocol.cc',
        'filters/blocking_url_protocol.h',
        'filters/chunk_demuxer.cc',
        'filters/chunk_demuxer.h',
        'filters/context_3d.h',
        'filters/decoder_selector.cc',
        'filters/decoder_selector.h',
        'filters/decoder_stream.cc',
        'filters/decoder_stream.h',
        'filters/decoder_stream_traits.cc',
        'filters/decoder_stream_traits.h',
        'filters/decrypting_audio_decoder.cc',
        'filters/decrypting_audio_decoder.h',
        'filters/decrypting_demuxer_stream.cc',
        'filters/decrypting_demuxer_stream.h',
        'filters/decrypting_video_decoder.cc',
        'filters/decrypting_video_decoder.h',
        'filters/default_media_permission.cc',
        'filters/default_media_permission.h',
        'filters/ffmpeg_audio_decoder.cc',
        'filters/ffmpeg_audio_decoder.h',
        'filters/ffmpeg_bitstream_converter.h',
        'filters/ffmpeg_demuxer.cc',
        'filters/ffmpeg_demuxer.h',
        'filters/ffmpeg_glue.cc',
        'filters/ffmpeg_glue.h',
        'filters/ffmpeg_video_decoder.cc',
        'filters/ffmpeg_video_decoder.h',
        'filters/file_data_source.cc',
        'filters/file_data_source.h',
        'filters/frame_processor.cc',
        'filters/frame_processor.h',
        'filters/gpu_video_decoder.cc',
        'filters/gpu_video_decoder.h',
        'filters/h264_bit_reader.cc',
        'filters/h264_bit_reader.h',
        'filters/h264_parser.cc',
        'filters/h264_parser.h',
        'filters/in_memory_url_protocol.cc',
        'filters/in_memory_url_protocol.h',
        'filters/ivf_parser.cc',
        'filters/ivf_parser.h',
        'filters/jpeg_parser.cc',
        'filters/jpeg_parser.h',
        'filters/media_source_state.cc',
        'filters/media_source_state.h',
        'filters/memory_data_source.cc',
        'filters/memory_data_source.h',
        'filters/opus_audio_decoder.cc',
        'filters/opus_audio_decoder.h',
        'filters/opus_constants.cc',
        'filters/opus_constants.h',
        'filters/pipeline_controller.cc',
        'filters/pipeline_controller.h',
        'filters/source_buffer_range.cc',
        'filters/source_buffer_range.h',
        'filters/source_buffer_stream.cc',
        'filters/source_buffer_stream.h',
        'filters/stream_parser_factory.cc',
        'filters/stream_parser_factory.h',
        'filters/video_cadence_estimator.cc',
        'filters/video_cadence_estimator.h',
        'filters/video_renderer_algorithm.cc',
        'filters/video_renderer_algorithm.h',
        'filters/vp8_bool_decoder.cc',
        'filters/vp8_bool_decoder.h',
        'filters/vp8_parser.cc',
        'filters/vp8_parser.h',
        'filters/vp9_parser.cc',
        'filters/vp9_parser.h',
        'filters/vp9_raw_bits_reader.cc',
        'filters/vp9_raw_bits_reader.h',
        'filters/vpx_video_decoder.cc',
        'filters/vpx_video_decoder.h',
        'filters/webvtt_util.h',
        'filters/wsola_internals.cc',
        'filters/wsola_internals.h',
        'formats/common/offset_byte_queue.cc',
        'formats/common/offset_byte_queue.h',
        'formats/webm/webm_audio_client.cc',
        'formats/webm/webm_audio_client.h',
        'formats/webm/webm_cluster_parser.cc',
        'formats/webm/webm_cluster_parser.h',
        'formats/webm/webm_constants.cc',
        'formats/webm/webm_constants.h',
        'formats/webm/webm_content_encodings.cc',
        'formats/webm/webm_content_encodings.h',
        'formats/webm/webm_content_encodings_client.cc',
        'formats/webm/webm_content_encodings_client.h',
        'formats/webm/webm_crypto_helpers.cc',
        'formats/webm/webm_crypto_helpers.h',
        'formats/webm/webm_info_parser.cc',
        'formats/webm/webm_info_parser.h',
        'formats/webm/webm_parser.cc',
        'formats/webm/webm_parser.h',
        'formats/webm/webm_stream_parser.cc',
        'formats/webm/webm_stream_parser.h',
        'formats/webm/webm_tracks_parser.cc',
        'formats/webm/webm_tracks_parser.h',
        'formats/webm/webm_video_client.cc',
        'formats/webm/webm_video_client.h',
        'formats/webm/webm_webvtt_parser.cc',
        'muxers/webm_muxer.cc',
        'muxers/webm_muxer.h',
        'renderers/audio_renderer_impl.cc',
        'renderers/audio_renderer_impl.h',
        'renderers/default_renderer_factory.cc',
        'renderers/default_renderer_factory.h',
        'renderers/gpu_video_accelerator_factories.h',
        'renderers/renderer_impl.cc',
        'renderers/renderer_impl.h',
        'renderers/skcanvas_video_renderer.cc',
        'renderers/skcanvas_video_renderer.h',
        'renderers/video_overlay_factory.cc',
        'renderers/video_overlay_factory.h',
        'renderers/video_renderer_impl.cc',
        'renderers/video_renderer_impl.h',
        'video/fake_video_encode_accelerator.cc',
        'video/fake_video_encode_accelerator.h',
        'video/gpu_memory_buffer_video_frame_pool.cc',
        'video/gpu_memory_buffer_video_frame_pool.h',
        'video/h264_poc.cc',
        'video/h264_poc.h',
        'video/jpeg_decode_accelerator.cc',
        'video/jpeg_decode_accelerator.h',
        'video/picture.cc',
        'video/picture.h',
        'video/video_decode_accelerator.cc',
        'video/video_decode_accelerator.h',
        'video/video_encode_accelerator.cc',
        'video/video_encode_accelerator.h',
        'formats/webm/webm_webvtt_parser.h'
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'conditions': [
        ['arm_neon==1', {
          'defines': [
            'USE_NEON'
          ],
        }],
        ['media_use_ffmpeg==1', {
          'dependencies': [
            '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
          ],
        }, {  # media_use_ffmpeg==0
          # Exclude the sources that depend on ffmpeg.
          'sources!': [
            'base/audio_video_metadata_extractor.cc',
            'base/audio_video_metadata_extractor.h',
            'base/media_file_checker.cc',
            'base/media_file_checker.h',
            'ffmpeg/ffmpeg_common.cc',
            'ffmpeg/ffmpeg_common.h',
            'filters/audio_file_reader.cc',
            'filters/audio_file_reader.h',
            'filters/blocking_url_protocol.cc',
            'filters/blocking_url_protocol.h',
            'filters/ffmpeg_aac_bitstream_converter.cc',
            'filters/ffmpeg_aac_bitstream_converter.h',
            'filters/ffmpeg_audio_decoder.cc',
            'filters/ffmpeg_audio_decoder.h',
            'filters/ffmpeg_bitstream_converter.h',
            'filters/ffmpeg_demuxer.cc',
            'filters/ffmpeg_demuxer.h',
            'filters/ffmpeg_glue.cc',
            'filters/ffmpeg_glue.h',
            'filters/ffmpeg_h264_to_annex_b_bitstream_converter.cc',
            'filters/ffmpeg_h264_to_annex_b_bitstream_converter.h',
            'filters/ffmpeg_video_decoder.cc',
            'filters/ffmpeg_video_decoder.h',
            'filters/in_memory_url_protocol.cc',
            'filters/in_memory_url_protocol.h',
          ],
          'defines': [
            'MEDIA_DISABLE_FFMPEG',
          ],
          'direct_dependent_settings': {
            'defines': [
              'MEDIA_DISABLE_FFMPEG',
            ],
          },
        }],
        ['media_use_libvpx==1', {
          'dependencies': [
            '<(DEPTH)/third_party/libvpx/libvpx.gyp:libvpx',
          ],
        }, {  # media_use_libvpx==0
          'defines': [
            'MEDIA_DISABLE_LIBVPX',
          ],
          'direct_dependent_settings': {
            'defines': [
              'MEDIA_DISABLE_LIBVPX',
            ],
          },
          # Exclude the sources that depend on libvpx.
          'sources!': [
            'filters/vpx_video_decoder.cc',
            'filters/vpx_video_decoder.h',
          ],
        }],
        ['OS=="android"', {
          'dependencies': [
            'media_android_jni_headers',
            'media_java',
            'player_android',
          ],
          'sources!': [
            'base/audio_video_metadata_extractor.cc',
            'base/audio_video_metadata_extractor.h',
            'base/media_file_checker.cc',
            'base/media_file_checker.h',
            'filters/decrypting_audio_decoder.cc',
            'filters/decrypting_audio_decoder.h',
            'filters/decrypting_video_decoder.cc',
            'filters/decrypting_video_decoder.h',
            'filters/ffmpeg_video_decoder.cc',
            'filters/ffmpeg_video_decoder.h',
          ],
          'sources': [
            'filters/android/media_codec_audio_decoder.cc',
            'filters/android/media_codec_audio_decoder.h',
          ],
          'defines': [
            'DISABLE_USER_INPUT_MONITOR',
          ],
          'conditions': [
            ['media_use_ffmpeg == 1', {
              'defines': [
                # On Android, FFmpeg is built without video decoders. We only
                # support hardware video decoding.
                'DISABLE_FFMPEG_VIDEO_DECODERS',
              ],
              'direct_dependent_settings': {
                'defines': [
                  'DISABLE_FFMPEG_VIDEO_DECODERS',
                ],
              },
            }],
          ],
        }],
        # For VaapiVideoEncodeAccelerator.
        ['target_arch != "arm" and chromeos == 1', {
          'sources': [
            'filters/h264_bitstream_buffer.cc',
            'filters/h264_bitstream_buffer.h',
          ],
        }],
        ['use_alsa==1', {
          'link_settings': {
            'libraries': [
              '-lasound',
            ],
          },
          'defines': [
            'USE_ALSA',
          ],
        }, { # use_alsa==0
          'sources/': [
            ['exclude', '(^|/)alsa/'],
            ['exclude', '_alsa\\.(h|cc)$'],
          ],
        }],
        ['OS=="linux"', {
          'conditions': [
            ['use_x11==1', {
              'dependencies': [
                '../build/linux/system.gyp:x11',
                '../build/linux/system.gyp:xdamage',
                '../build/linux/system.gyp:xext',
                '../build/linux/system.gyp:xfixes',
                '../build/linux/system.gyp:xtst',
                '../ui/events/keycodes/events_keycodes.gyp:keycodes_x11',
                '../ui/gfx/x/gfx_x11.gyp:gfx_x11',
              ],
            }, {  # else: use_x11==0
              'sources!': [
                'base/user_input_monitor_linux.cc',
              ],
              'defines': [
                'DISABLE_USER_INPUT_MONITOR',
              ],
            }],
            ['use_cras==1', {
              'dependencies': [
                '../chromeos/chromeos.gyp:chromeos',
              ],
              'cflags': [
                '<!@(<(pkg-config) --cflags libcras)',
              ],
              'link_settings': {
                'libraries': [
                  '<!@(<(pkg-config) --libs libcras)',
                ],
              },
              'defines': [
                'USE_CRAS',
              ],
            }, {  # else: use_cras==0
              'sources!': [
                'audio/cras/audio_manager_cras.cc',
                'audio/cras/audio_manager_cras.h',
                'audio/cras/cras_input.cc',
                'audio/cras/cras_input.h',
                'audio/cras/cras_unified.cc',
                'audio/cras/cras_unified.h',
              ],
            }],
          ],
        }],
        ['OS!="linux"', {
          'sources!': [
            'audio/cras/audio_manager_cras.cc',
            'audio/cras/audio_manager_cras.h',
            'audio/cras/cras_input.cc',
            'audio/cras/cras_input.h',
            'audio/cras/cras_unified.cc',
            'audio/cras/cras_unified.h',
          ],
        }],
        ['use_pulseaudio==1', {
          'cflags': [
            '<!@(<(pkg-config) --cflags libpulse)',
          ],
          'defines': [
            'USE_PULSEAUDIO',
          ],
          'conditions': [
            ['linux_link_pulseaudio==0', {
              'defines': [
                'DLOPEN_PULSEAUDIO',
              ],
              'variables': {
                'generate_stubs_script': '../tools/generate_stubs/generate_stubs.py',
                'extra_header': 'audio/pulse/pulse_stub_header.fragment',
                'sig_files': ['audio/pulse/pulse.sigs'],
                'outfile_type': 'posix_stubs',
                'stubs_filename_root': 'pulse_stubs',
                'project_path': 'media/audio/pulse',
                'intermediate_dir': '<(INTERMEDIATE_DIR)',
                'output_root': '<(SHARED_INTERMEDIATE_DIR)/pulse',
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
                  'message': 'Generating Pulse stubs for dynamic loading',
                },
              ],
              'conditions': [
                # Linux/Solaris need libdl for dlopen() and friends.
                ['OS=="linux" or OS=="solaris"', {
                  'link_settings': {
                    'libraries': [
                      '-ldl',
                    ],
                  },
                }],
              ],
            }, {  # else: linux_link_pulseaudio==0
              'link_settings': {
                'ldflags': [
                  '<!@(<(pkg-config) --libs-only-L --libs-only-other libpulse)',
                ],
                'libraries': [
                  '<!@(<(pkg-config) --libs-only-l libpulse)',
                ],
              },
            }],
          ],
        }, {  # else: use_pulseaudio==0
          'sources!': [
            'audio/pulse/audio_manager_pulse.cc',
            'audio/pulse/audio_manager_pulse.h',
            'audio/pulse/pulse_input.cc',
            'audio/pulse/pulse_input.h',
            'audio/pulse/pulse_output.cc',
            'audio/pulse/pulse_output.h',
            'audio/pulse/pulse_util.cc',
            'audio/pulse/pulse_util.h',
          ],
        }],
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AudioToolbox.framework',
              '$(SDKROOT)/System/Library/Frameworks/AudioUnit.framework',
              '$(SDKROOT)/System/Library/Frameworks/AVFoundation.framework',
              '$(SDKROOT)/System/Library/Frameworks/CoreAudio.framework',
              '$(SDKROOT)/System/Library/Frameworks/CoreVideo.framework',
              '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
            ],
          },
        }],
        ['OS=="win"', {
          # TODO(wolenetz): Fix size_t to int truncations in win64. See
          # http://crbug.com/171009
          'conditions': [
            ['target_arch=="x64"', {
              'msvs_disabled_warnings': [ 4267, ],
            }],
          ],
        }],
        ['proprietary_codecs==1', {
          'sources': [
            'cdm/cenc_utils.cc',
            'cdm/cenc_utils.h',
            'filters/ffmpeg_aac_bitstream_converter.cc',
            'filters/ffmpeg_aac_bitstream_converter.h',
            'filters/ffmpeg_h264_to_annex_b_bitstream_converter.cc',
            'filters/ffmpeg_h264_to_annex_b_bitstream_converter.h',
            'filters/h264_to_annex_b_bitstream_converter.cc',
            'filters/h264_to_annex_b_bitstream_converter.h',
            'formats/mp4/aac.cc',
            'formats/mp4/aac.h',
            'formats/mp4/avc.cc',
            'formats/mp4/avc.h',
            'formats/mp4/bitstream_converter.cc',
            'formats/mp4/bitstream_converter.h',
            'formats/mp4/box_definitions.cc',
            'formats/mp4/box_definitions.h',
            'formats/mp4/box_reader.cc',
            'formats/mp4/box_reader.h',
            'formats/mp4/es_descriptor.cc',
            'formats/mp4/es_descriptor.h',
            'formats/mp4/mp4_stream_parser.cc',
            'formats/mp4/mp4_stream_parser.h',
            'formats/mp4/sample_to_group_iterator.cc',
            'formats/mp4/sample_to_group_iterator.h',
            'formats/mp4/track_run_iterator.cc',
            'formats/mp4/track_run_iterator.h',
            'formats/mpeg/adts_constants.cc',
            'formats/mpeg/adts_constants.h',
            'formats/mpeg/adts_header_parser.cc',
            'formats/mpeg/adts_header_parser.h',
            'formats/mpeg/adts_stream_parser.cc',
            'formats/mpeg/adts_stream_parser.h',
            'formats/mpeg/mpeg1_audio_stream_parser.cc',
            'formats/mpeg/mpeg1_audio_stream_parser.h',
            'formats/mpeg/mpeg_audio_stream_parser_base.cc',
            'formats/mpeg/mpeg_audio_stream_parser_base.h',
          ],
        }],
        ['proprietary_codecs==1 and enable_mse_mpeg2ts_stream_parser==1', {
          'sources': [
            'formats/mp2t/es_adapter_video.cc',
            'formats/mp2t/es_adapter_video.h',
            'formats/mp2t/es_parser.cc',
            'formats/mp2t/es_parser.h',
            'formats/mp2t/es_parser_adts.cc',
            'formats/mp2t/es_parser_adts.h',
            'formats/mp2t/es_parser_h264.cc',
            'formats/mp2t/es_parser_h264.h',
            'formats/mp2t/es_parser_mpeg1audio.cc',
            'formats/mp2t/es_parser_mpeg1audio.h',
            'formats/mp2t/mp2t_common.h',
            'formats/mp2t/mp2t_stream_parser.cc',
            'formats/mp2t/mp2t_stream_parser.h',
            'formats/mp2t/timestamp_unroller.cc',
            'formats/mp2t/timestamp_unroller.h',
            'formats/mp2t/ts_packet.cc',
            'formats/mp2t/ts_packet.h',
            'formats/mp2t/ts_section.h',
            'formats/mp2t/ts_section_pat.cc',
            'formats/mp2t/ts_section_pat.h',
            'formats/mp2t/ts_section_pes.cc',
            'formats/mp2t/ts_section_pes.h',
            'formats/mp2t/ts_section_pmt.cc',
            'formats/mp2t/ts_section_pmt.h',
            'formats/mp2t/ts_section_psi.cc',
            'formats/mp2t/ts_section_psi.h',
          ],
        }],
        ['proprietary_codecs==1 and enable_hevc_demuxing==1', {
          'sources': [
            'filters/h265_parser.cc',
            'filters/h265_parser.h',
            'formats/mp4/hevc.cc',
            'formats/mp4/hevc.h',
          ],
        }],
        ['proprietary_codecs==1 and enable_hevc_demuxing==1 and media_use_ffmpeg==1', {
          'sources': [
            'filters/ffmpeg_h265_to_annex_b_bitstream_converter.cc',
            'filters/ffmpeg_h265_to_annex_b_bitstream_converter.h',
          ],
        }],
        ['target_arch=="ia32" or target_arch=="x64"', {
          'dependencies': [
            'media_asm',
          ],
          'sources': [
            'base/simd/convert_rgb_to_yuv_sse2.cc',
            'base/simd/convert_rgb_to_yuv_ssse3.cc',
            'base/simd/convert_yuv_to_rgb_x86.cc',
            'base/simd/filter_yuv_sse2.cc',
          ],
        }],
        ['OS!="linux" and OS!="win"', {
          'sources!': [
            'base/keyboard_event_counter.cc',
            'base/keyboard_event_counter.h',
          ],
        }],
        ['use_low_memory_buffer==1', {
          'sources': [
            'filters/source_buffer_platform.h',
            'filters/source_buffer_platform_lowmem.cc',
          ]
        }, {  # 'use_low_memory_buffer==0'
          'sources': [
            'filters/source_buffer_platform.cc',
            'filters/source_buffer_platform.h',
          ]
        }],
      ],  # conditions
      'target_conditions': [
        ['OS == "ios" and _toolset != "host"', {
          'sources/': [
            # Pull in specific Mac files for iOS (which have been filtered out
            # by file name rules).
            ['include', '^base/mac/coremedia_glue\\.h$'],
            ['include', '^base/mac/coremedia_glue\\.mm$'],
            ['include', '^base/mac/corevideo_glue\\.h$'],
            ['include', '^base/mac/videotoolbox_glue\\.h$'],
            ['include', '^base/mac/videotoolbox_glue\\.mm$'],
            ['include', '^base/mac/video_frame_mac\\.h$'],
            ['include', '^base/mac/video_frame_mac\\.cc$'],
          ],
        }],
      ],  # target_conditions
    },
    {
      # GN version: //media:cdm_paths
      'target_name': 'cdm_paths',
      'type': 'static_library',
      'sources': [
        'cdm/cdm_paths.cc',
        'cdm/cdm_paths.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ]
    },
    {
      # GN version: //media:media_unittests
      'target_name': 'media_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'audio_test_config',
        'cdm_paths',
        'media',
        'media_features',
        'media_test_support',
        'shared_memory_support',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../gpu/gpu.gyp:command_buffer_common',
        '../gpu/gpu.gyp:gpu_unittest_utils',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/libwebm/libwebm.gyp:libwebm',
        '../third_party/libyuv/libyuv.gyp:libyuv',
        '../third_party/widevine/cdm/widevine_cdm.gyp:widevine_cdm_version_h',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../ui/gfx/gfx.gyp:gfx_test_support',
        '../url/url.gyp:url_lib',
      ],
      'sources': [
        'base/android/access_unit_queue_unittest.cc',
        'base/android/media_codec_decoder_unittest.cc',
        'base/android/media_codec_loop_unittest.cc',
        'base/android/media_drm_bridge_unittest.cc',
        'base/android/media_player_bridge_unittest.cc',
        'base/android/media_source_player_unittest.cc',
        'base/android/sdk_media_codec_bridge_unittest.cc',
        'base/android/test_data_factory.cc',
        'base/android/test_data_factory.h',
        'base/android/test_statistics.h',
        'base/audio_block_fifo_unittest.cc',
        'base/audio_buffer_converter_unittest.cc',
        'base/audio_buffer_queue_unittest.cc',
        'base/audio_buffer_unittest.cc',
        'base/audio_bus_unittest.cc',
        'base/audio_converter_unittest.cc',
        'base/audio_discard_helper_unittest.cc',
        'base/audio_fifo_unittest.cc',
        'base/audio_hardware_config_unittest.cc',
        'base/audio_hash_unittest.cc',
	'base/audio_latency_unittest.cc',
        'base/audio_parameters_unittest.cc',
        'base/audio_point_unittest.cc',
        'base/audio_pull_fifo_unittest.cc',
        'base/audio_push_fifo_unittest.cc',
        'base/audio_renderer_mixer_input_unittest.cc',
        'base/audio_renderer_mixer_unittest.cc',
        'base/audio_sample_types_unittest.cc',
        'base/audio_shifter_unittest.cc',
        'base/audio_splicer_unittest.cc',
        'base/audio_timestamp_helper_unittest.cc',
        'base/audio_video_metadata_extractor_unittest.cc',
        'base/bind_to_current_loop_unittest.cc',
        'base/bit_reader_unittest.cc',
        'base/callback_holder.h',
        'base/callback_holder_unittest.cc',
        'base/channel_mixer_unittest.cc',
        'base/channel_mixing_matrix_unittest.cc',
        'base/container_names_unittest.cc',
        'base/data_buffer_unittest.cc',
        'base/decoder_buffer_queue_unittest.cc',
        'base/decoder_buffer_unittest.cc',
        'base/djb2_unittest.cc',
        'base/fake_demuxer_stream_unittest.cc',
        'base/gmock_callback_support_unittest.cc',
        'base/key_systems_unittest.cc',
        'base/mac/video_frame_mac_unittests.cc',
        'base/media_file_checker_unittest.cc',
        'base/mime_util_unittest.cc',
        'base/moving_average_unittest.cc',
        'base/multi_channel_resampler_unittest.cc',
        'base/null_video_sink_unittest.cc',
        'base/pipeline_impl_unittest.cc',
        'base/ranges_unittest.cc',
        'base/run_all_unittests.cc',
        'base/seekable_buffer_unittest.cc',
        'base/serial_runner_unittest.cc',
        'base/sinc_resampler_unittest.cc',
        'base/stream_parser_unittest.cc',
        'base/text_ranges_unittest.cc',
        'base/text_renderer_unittest.cc',
        'base/time_delta_interpolator_unittest.cc',
        'base/user_input_monitor_unittest.cc',
        'base/vector_math_testing.h',
        'base/vector_math_unittest.cc',
        'base/video_decoder_config_unittest.cc',
        'base/video_frame_pool_unittest.cc',
        'base/video_frame_unittest.cc',
        'base/video_util_unittest.cc',
        'base/wall_clock_time_source_unittest.cc',
        'base/yuv_convert_unittest.cc',
        'cdm/aes_decryptor_unittest.cc',
        'cdm/external_clear_key_test_helper.cc',
        'cdm/external_clear_key_test_helper.h',
        'cdm/json_web_key_unittest.cc',
        'cdm/simple_cdm_allocator.cc',
        'cdm/simple_cdm_allocator.h',
        'cdm/simple_cdm_allocator_unittest.cc',
        'cdm/simple_cdm_buffer.cc',
        'cdm/simple_cdm_buffer.h',
        'ffmpeg/ffmpeg_common_unittest.cc',
        'filters/audio_clock_unittest.cc',
        'filters/audio_decoder_selector_unittest.cc',
        'filters/audio_decoder_unittest.cc',
        'filters/audio_file_reader_unittest.cc',
        'filters/audio_renderer_algorithm_unittest.cc',
        'filters/audio_timestamp_validator_unittest.cc',
        'filters/blocking_url_protocol_unittest.cc',
        'filters/chunk_demuxer_unittest.cc',
        'filters/decrypting_audio_decoder_unittest.cc',
        'filters/decrypting_demuxer_stream_unittest.cc',
        'filters/decrypting_video_decoder_unittest.cc',
        'filters/fake_video_decoder.cc',
        'filters/fake_video_decoder.h',
        'filters/fake_video_decoder_unittest.cc',
        'filters/ffmpeg_demuxer_unittest.cc',
        'filters/ffmpeg_glue_unittest.cc',
        'filters/ffmpeg_video_decoder_unittest.cc',
        'filters/file_data_source_unittest.cc',
        'filters/frame_processor_unittest.cc',
        'filters/h264_bit_reader_unittest.cc',
        'filters/h264_parser_unittest.cc',
        'filters/in_memory_url_protocol_unittest.cc',
        'filters/ivf_parser_unittest.cc',
        'filters/jpeg_parser_unittest.cc',
        'filters/memory_data_source_unittest.cc',
        'filters/pipeline_controller_unittest.cc',
        'filters/source_buffer_stream_unittest.cc',
        'filters/video_cadence_estimator_unittest.cc',
        'filters/video_decoder_selector_unittest.cc',
        'filters/video_frame_stream_unittest.cc',
        'filters/video_renderer_algorithm_unittest.cc',
        'filters/vp8_bool_decoder_unittest.cc',
        'filters/vp8_parser_unittest.cc',
        'filters/vp9_parser_unittest.cc',
        'filters/vp9_raw_bits_reader_unittest.cc',
        'formats/common/offset_byte_queue_unittest.cc',
        'formats/webm/cluster_builder.cc',
        'formats/webm/cluster_builder.h',
        'formats/webm/opus_packet_builder.cc',
        'formats/webm/opus_packet_builder.h',
        'formats/webm/tracks_builder.cc',
        'formats/webm/tracks_builder.h',
        'formats/webm/webm_cluster_parser_unittest.cc',
        'formats/webm/webm_content_encodings_client_unittest.cc',
        'formats/webm/webm_parser_unittest.cc',
        'formats/webm/webm_stream_parser_unittest.cc',
        'formats/webm/webm_tracks_parser_unittest.cc',
        'formats/webm/webm_webvtt_parser_unittest.cc',
        'muxers/webm_muxer_unittest.cc',
        'renderers/audio_renderer_impl_unittest.cc',
        'renderers/renderer_impl_unittest.cc',
        'renderers/skcanvas_video_renderer_unittest.cc',
        'renderers/video_renderer_impl_unittest.cc',
        'test/pipeline_integration_test.cc',
        'test/pipeline_integration_test_base.cc',
        'video/gpu_memory_buffer_video_frame_pool_unittest.cc',
        'video/h264_poc_unittest.cc',
      ],
      'include_dirs': [
        # Needed by media_drm_bridge.cc.
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'conditions': [
        ['arm_neon==1', {
          'defines': [
            'USE_NEON'
          ],
        }],
        ['proprietary_codecs==1 and enable_hevc_demuxing==1', {
          'sources': [
            'filters/h265_parser_unittest.cc',
          ],
        }],
        ['media_use_ffmpeg==1', {
          'dependencies': [
            '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
          ],
        }, {  # media_use_ffmpeg==0
          'sources!': [
            'ffmpeg/ffmpeg_common_unittest.cc',
            'filters/audio_decoder_unittest.cc',
            'filters/audio_file_reader_unittest.cc',
            'filters/blocking_url_protocol_unittest.cc',
            'filters/ffmpeg_aac_bitstream_converter_unittest.cc',
            'filters/ffmpeg_demuxer_unittest.cc',
            'filters/ffmpeg_glue_unittest.cc',
            'filters/ffmpeg_h264_to_annex_b_bitstream_converter_unittest.cc',
            'filters/in_memory_url_protocol_unittest.cc'
          ],
        }],
        # Even if FFmpeg is enabled on Android we don't want these.
        # TODO(watk): Refactor tests that could be made to run on Android. See
        # http://crbug.com/570762
        ['media_use_ffmpeg==0 or OS=="android"', {
          'sources!': [
            'base/audio_video_metadata_extractor_unittest.cc',
            'base/media_file_checker_unittest.cc',
            'filters/ffmpeg_video_decoder_unittest.cc',
            'test/pipeline_integration_test.cc',
            'test/pipeline_integration_test_base.cc',
          ],
        }],
        ['OS=="android"', {
          'dependencies': [
            '../testing/android/native_test.gyp:native_test_native_code',
            'player_android',
          ],
          'sources!': [
            'filters/decrypting_audio_decoder_unittest.cc',
            'filters/decrypting_video_decoder_unittest.cc',
          ],
        }],
        # If ExternalClearKey is built, we can test CdmAdapter.
        ['enable_pepper_cdms == 1', {
          'dependencies': [
            'clearkeycdm',
          ],
          'sources': [
            'cdm/cdm_adapter_unittest.cc',
          ],
          'conditions': [
            ['OS == "mac"', {
              'xcode_settings': {
                'LD_RUNPATH_SEARCH_PATHS' : [ '@executable_path/<(clearkey_cdm_path)' ],
              },
            }]
           ],
        }],
        ['target_arch != "arm" and chromeos == 1 and use_x11 == 1', {
          'sources': [
            'filters/h264_bitstream_buffer_unittest.cc',
          ],
        }],
        ['target_arch=="ia32" or target_arch=="x64"', {
          'sources': [
            'base/simd/convert_rgb_to_yuv_unittest.cc',
          ],
        }],
        ['proprietary_codecs==1', {
          'sources': [
            'base/android/media_codec_player_unittest.cc',
            'cdm/cenc_utils_unittest.cc',
            'filters/ffmpeg_aac_bitstream_converter_unittest.cc',
            'filters/ffmpeg_h264_to_annex_b_bitstream_converter_unittest.cc',
            'filters/h264_to_annex_b_bitstream_converter_unittest.cc',
            'formats/common/stream_parser_test_base.cc',
            'formats/common/stream_parser_test_base.h',
            'formats/mp4/aac_unittest.cc',
            'formats/mp4/avc_unittest.cc',
            'formats/mp4/box_reader_unittest.cc',
            'formats/mp4/es_descriptor_unittest.cc',
            'formats/mp4/mp4_stream_parser_unittest.cc',
            'formats/mp4/sample_to_group_iterator_unittest.cc',
            'formats/mp4/track_run_iterator_unittest.cc',
            'formats/mpeg/adts_stream_parser_unittest.cc',
            'formats/mpeg/mpeg1_audio_stream_parser_unittest.cc',
          ],
        }],
        ['proprietary_codecs==1 and enable_mse_mpeg2ts_stream_parser==1', {
          'sources': [
            'formats/mp2t/es_adapter_video_unittest.cc',
            'formats/mp2t/es_parser_adts_unittest.cc',
            'formats/mp2t/es_parser_h264_unittest.cc',
            'formats/mp2t/es_parser_mpeg1audio_unittest.cc',
            'formats/mp2t/es_parser_test_base.cc',
            'formats/mp2t/es_parser_test_base.h',
            'formats/mp2t/mp2t_stream_parser_unittest.cc',
            'formats/mp2t/timestamp_unroller_unittest.cc',
          ],
        }],
        ['use_x11==1', {
          'dependencies': [
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
      ],
    },
    {
      # GN version: //media:media_perftests
      'target_name': 'media_perftests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../base/base.gyp:test_support_base',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../testing/perf/perf_test.gyp:perf_test',
        '../third_party/libyuv/libyuv.gyp:libyuv',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../ui/gfx/gfx.gyp:gfx_test_support',
        'media',
        'media_features',
        'media_test_support',
        'shared_memory_support',
      ],
      'sources': [
        'base/audio_bus_perftest.cc',
        'base/audio_converter_perftest.cc',
        'base/demuxer_perftest.cc',
        'base/run_all_perftests.cc',
        'base/sinc_resampler_perftest.cc',
        'base/vector_math_perftest.cc',
        'base/yuv_convert_perftest.cc',
        'test/pipeline_integration_perftest.cc',
        'test/pipeline_integration_test_base.cc',
      ],
      'conditions': [
        ['arm_neon==1', {
          'defines': [
            'USE_NEON'
          ],
        }],
        ['OS=="android" or media_use_ffmpeg==0', {
          # TODO(watk): Refactor tests that could be made to run on Android.
          # See http://crbug.com/570762
          'sources!': [
            'base/demuxer_perftest.cc',
            'test/pipeline_integration_perftest.cc',
            'test/pipeline_integration_test_base.cc',
          ],
        }],
        ['OS=="android"', {
          'dependencies': [
            '../testing/android/native_test.gyp:native_test_native_code',
            '../ui/gl/gl.gyp:gl',
          ],
        }],
        ['media_use_ffmpeg==1', {
          'dependencies': [
            '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
          ],
        }],
      ],
    },
    {
      # GN version: //media/audio:unittests
      # For including the sources and configs in multiple test targets.
      'target_name': 'audio_test_config',
      'type': 'none',
      'direct_dependent_settings': {
        'sources': [
          'audio/audio_input_controller_unittest.cc',
          'audio/audio_input_unittest.cc',
          'audio/audio_manager_unittest.cc',
          'audio/audio_output_controller_unittest.cc',
          'audio/audio_output_device_unittest.cc',
          'audio/audio_output_proxy_unittest.cc',
          'audio/audio_power_monitor_unittest.cc',
          'audio/audio_streams_tracker_unittest.cc',
          'audio/fake_audio_worker_unittest.cc',
          'audio/simple_sources_unittest.cc',
          'audio/virtual_audio_input_stream_unittest.cc',
          'audio/virtual_audio_output_stream_unittest.cc',
        ],
        'conditions': [
          # TODO(wolenetz): Fix size_t to int truncations in win64. See
          # http://crbug.com/171009
          ['OS=="win" and target_arch=="x64"', {
            'msvs_disabled_warnings': [ 4267, ],
          }],
          ['OS=="android"', {
            'sources': [
              'audio/android/audio_android_unittest.cc',
            ],
          }],
          ['OS=="mac"', {
            'sources': [
              'audio/mac/audio_auhal_mac_unittest.cc',
              'audio/mac/audio_device_listener_mac_unittest.cc',
              'audio/mac/audio_low_latency_input_mac_unittest.cc',
            ],
          }],
          ['chromeos==1 or chromecast==1', {
            'sources': [
              'audio/sounds/audio_stream_handler_unittest.cc',
              'audio/sounds/sounds_manager_unittest.cc',
              'audio/sounds/test_data.cc',
              'audio/sounds/test_data.h',
              'audio/sounds/wav_audio_handler_unittest.cc',
            ],
          }],
          ['OS=="win"', {
            'sources': [
              'audio/win/audio_device_listener_win_unittest.cc',
              'audio/win/audio_low_latency_input_win_unittest.cc',
              'audio/win/audio_low_latency_output_win_unittest.cc',
              'audio/win/audio_output_win_unittest.cc',
              'audio/win/core_audio_util_win_unittest.cc',
            ],
          }],
          ['use_alsa==1', {
            'sources': [
              'audio/alsa/alsa_output_unittest.cc',
              'audio/audio_low_latency_input_output_unittest.cc',
            ],
            'defines': [
              'USE_ALSA',
            ],
          }],
          ['use_pulseaudio==1', {
            'defines': [
              'USE_PULSEAUDIO',
            ],
          }],
          ['use_cras==1', {
            'sources': [
              'audio/cras/cras_input_unittest.cc',
              'audio/cras/cras_unified_unittest.cc',
            ],
            'defines': [
              'USE_CRAS',
            ],
          }],
        ],
      },
    },
    {
      # GN version: //media:audio_unittests
      # For running the subset of tests that might require audio
      # hardware separately on GPU bots. media_unittests includes these too.
      'target_name': 'audio_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'audio_test_config',
        'media_test_support',
        '../base/base.gyp:test_support_base',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../ui/gfx/gfx.gyp:gfx_test_support',
        '../url/url.gyp:url_lib',
      ],
      'sources': [
        'base/run_all_unittests.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'link_settings':  {
            'libraries': [
              '-ldxguid.lib',
              '-lsetupapi.lib',
              '-lwinmm.lib',
            ],
          },
        }],
      ],
    },
    {
      # GN versions (it is split apart): //media:test_support,
      # //media/base:test_support, and //media/audio:test_support
      'target_name': 'media_test_support',
      'type': 'static_library',
      'dependencies': [
        'media',
        'media_features',
        'shared_memory_support',
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../ui/gfx/gfx.gyp:gfx_test_support',
      ],
      'sources': [
        'audio/audio_unittest_util.cc',
        'audio/audio_unittest_util.h',
        'audio/mock_audio_manager.cc',
        'audio/mock_audio_manager.h',
        'audio/mock_audio_source_callback.cc',
        'audio/mock_audio_source_callback.h',
        'audio/test_audio_input_controller_factory.cc',
        'audio/test_audio_input_controller_factory.h',
        'base/fake_audio_render_callback.cc',
        'base/fake_audio_render_callback.h',
        'base/fake_audio_renderer_sink.cc',
        'base/fake_audio_renderer_sink.h',
        'base/fake_demuxer_stream.cc',
        'base/fake_demuxer_stream.h',
        'base/fake_media_resources.cc',
        'base/fake_media_resources.h',
        'base/fake_single_thread_task_runner.cc',
        'base/fake_single_thread_task_runner.h',
        'base/fake_text_track_stream.cc',
        'base/fake_text_track_stream.h',
        'base/gmock_callback_support.h',
        'base/mock_audio_renderer_sink.cc',
        'base/mock_audio_renderer_sink.h',
        'base/mock_demuxer_host.cc',
        'base/mock_demuxer_host.h',
        'base/mock_filters.cc',
        'base/mock_filters.h',
        'base/mock_media_log.cc',
        'base/mock_media_log.h',
        'base/test_data_util.cc',
        'base/test_data_util.h',
        'base/test_helpers.cc',
        'base/test_helpers.h',
        'base/test_random.h',
        'renderers/mock_gpu_memory_buffer_video_frame_pool.cc',
        'renderers/mock_gpu_memory_buffer_video_frame_pool.h',
        'renderers/mock_gpu_video_accelerator_factories.cc',
        'renderers/mock_gpu_video_accelerator_factories.h',
        'video/mock_video_decode_accelerator.cc',
        'video/mock_video_decode_accelerator.h',
      ],
    },
    {
      # Minimal target for NaCl and other renderer side media clients which
      # only need to send audio data across the shared memory to the browser
      # process.
      # GN version: //media:shared_memory_support
      'target_name': 'shared_memory_support',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../ui/gfx/gfx.gyp:gfx_geometry',
      ],
      'defines': [
        'MEDIA_IMPLEMENTATION',
      ],
      'include_dirs': [
        '..',
      ],
      'includes': [
        'shared_memory_support.gypi',
      ],
      'sources': [
        '<@(shared_memory_support_sources)',
      ],
      'conditions': [
        ['arm_neon==1', {
          'defines': [
            'USE_NEON'
          ],
        }],
      ],
    },
    {
      # GN version: //media/gpu
      'target_name': 'media_gpu',
      'type': '<(component)',
      'includes': [ 'media_gpu.gypi' ],
    },
  ],
  'conditions': [
    ['target_arch=="ia32" or target_arch=="x64"', {
      'targets': [
       {
          'target_name': 'media_asm',
          'type': 'static_library',
          'sources': [
            'base/simd/convert_rgb_to_yuv_ssse3.asm',
            'base/simd/convert_yuv_to_rgb_sse.asm',
            'base/simd/convert_yuva_to_argb_mmx.asm',
            'base/simd/empty_register_state_mmx.asm',
            'base/simd/linear_scale_yuv_to_rgb_mmx.asm',
            'base/simd/linear_scale_yuv_to_rgb_sse.asm',
            'base/simd/scale_yuv_to_rgb_mmx.asm',
            'base/simd/scale_yuv_to_rgb_sse.asm',
          ],
          'conditions': [
            ['component=="shared_library"', {
              'variables': {
                'yasm_flags': ['-DEXPORT_SYMBOLS'],
              },
            }],
            ['target_arch=="x64"', {
              # Source files optimized for X64 systems.
              'sources': [
                'base/simd/linear_scale_yuv_to_rgb_mmx_x64.asm',
                'base/simd/scale_yuv_to_rgb_sse2_x64.asm',
              ],
              'variables': {
                'yasm_flags': ['-DARCH_X86_64'],
              },
            }],
            ['OS=="mac" or OS=="ios"', {
              'variables': {
                'yasm_flags': [
                  '-DPREFIX',
                  '-DMACHO',
                ],
              },
              'sources': [
                # XCode doesn't want to link a pure assembly target and will
                # fail to link when it creates an empty file list.  So add a
                # dummy file keep the linker happy.  See http://crbug.com/157073
                'base/simd/xcode_hack.c',
              ],
            }],
            ['os_posix==1 and OS!="mac"', {
              'variables': {
                'conditions': [
                  ['target_arch=="ia32"', {
                    'yasm_flags': [
                      '-DARCH_X86_32',
                      '-DELF',
                    ],
                  }, { # target_arch=="x64"
                    'yasm_flags': [
                      '-DARCH_X86_64',
                      '-DELF',
                      '-DPIC',
                    ],
                  }],
                ],
              },
            }],
          ],
          'variables': {
            'yasm_output_path': '<(SHARED_INTERMEDIATE_DIR)/media',
            'yasm_flags': [
              '-DCHROMIUM',
              # In addition to the same path as source asm, let yasm %include
              # search path be relative to src/ per Chromium policy.
              '-I..',
            ],
            'yasm_includes': [
              '../third_party/x86inc/x86inc.asm',
              'base/simd/convert_rgb_to_yuv_ssse3.inc',
              'base/simd/convert_yuv_to_rgb_mmx.inc',
              'base/simd/convert_yuva_to_argb_mmx.inc',
              'base/simd/linear_scale_yuv_to_rgb_mmx.inc',
              'base/simd/media_export.asm',
              'base/simd/scale_yuv_to_rgb_mmx.inc',
            ],
          },
          'msvs_2010_disable_uldi_when_referenced': 1,
          'includes': [
            '../third_party/yasm/yasm_compile.gypi',
          ],
        },
      ], # targets
    }],
    ['OS=="win"', {
      'targets': [
        {
          # GN version: //media/base/win
          'target_name': 'mf_initializer',
          'type': '<(component)',
          'include_dirs': [ '..', ],
          'defines': [ 'MF_INITIALIZER_IMPLEMENTATION', ],
          'sources': [
            'base/win/mf_initializer_export.h',
            'base/win/mf_initializer.cc',
            'base/win/mf_initializer.h',
          ],
          'dependencies': [
            '../base/base.gyp:base',
          ],
          'link_settings':  {
            'libraries': [
              '-ldxguid.lib',
              '-lmf.lib',
              '-lmfplat.lib',
              '-lmfreadwrite.lib',
              '-lmfuuid.lib',
              '-lsetupapi.lib',
              '-lwinmm.lib',
            ],
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'DelayLoadDLLs': [
                'mf.dll',
                'mfplat.dll',
                'mfreadwrite.dll',
              ],
            },
          },
          'all_dependent_settings': {
            'msvs_settings': {
              'VCLinkerTool': {
                'DelayLoadDLLs': [
                  'mf.dll',
                  'mfplat.dll',
                  'mfreadwrite.dll',
                ],
              },
            },
          },
        },
      ],
    }],
    ['OS=="android"', {
      'targets': [
        {
          # TODO(GN)
          'target_name': 'media_unittests_apk',
          'type': 'none',
          'dependencies': [
            'media_java',
            'media_unittests',
          ],
          'variables': {
            'test_suite_name': 'media_unittests',
            'isolate_file': 'media_unittests.isolate',
          },
          'includes': ['../build/apk_test.gypi'],
        },
        {
          # TODO(GN)
          'target_name': 'media_perftests_apk',
          'type': 'none',
          'dependencies': [
            'media_java',
            'media_perftests',
          ],
          'variables': {
            'test_suite_name': 'media_perftests',
            'isolate_file': 'media_perftests.isolate',
          },
          'includes': ['../build/apk_test.gypi'],
        },
        {
          # GN: //media/base/android:media_jni_headers
          'target_name': 'media_android_jni_headers',
          'type': 'none',
          'sources': [
            'base/android/java/src/org/chromium/media/AudioManagerAndroid.java',
            'base/android/java/src/org/chromium/media/AudioRecordInput.java',
            'base/android/java/src/org/chromium/media/MediaCodecBridge.java',
            'base/android/java/src/org/chromium/media/MediaCodecUtil.java',
            'base/android/java/src/org/chromium/media/MediaDrmBridge.java',
            'base/android/java/src/org/chromium/media/MediaPlayerBridge.java',
            'base/android/java/src/org/chromium/media/MediaPlayerListener.java',
          ],
          'variables': {
            'jni_gen_package': 'media',
          },
          'includes': ['../build/jni_generator.gypi'],
        },
        {
          # GN: //media/base/android:android
          'target_name': 'player_android',
          'type': 'static_library',
          'sources': [
            'base/android/access_unit_queue.cc',
            'base/android/access_unit_queue.h',
            'base/android/android_cdm_factory.cc',
            'base/android/android_cdm_factory.h',
            'base/android/audio_decoder_job.cc',
            'base/android/audio_decoder_job.h',
            'base/android/audio_media_codec_decoder.cc',
            'base/android/audio_media_codec_decoder.h',
            'base/android/demuxer_android.h',
            'base/android/demuxer_stream_player_params.cc',
            'base/android/demuxer_stream_player_params.h',
            'base/android/media_client_android.cc',
            'base/android/media_client_android.h',
            'base/android/media_codec_bridge.cc',
            'base/android/media_codec_bridge.h',
            'base/android/media_codec_decoder.cc',
            'base/android/media_codec_decoder.h',
            'base/android/media_codec_loop.cc',
            'base/android/media_codec_loop.h',
            'base/android/media_codec_player.cc',
            'base/android/media_codec_player.h',
            'base/android/media_codec_util.cc',
            'base/android/media_codec_util.h',
            'base/android/media_common_android.h',
            'base/android/media_decoder_job.cc',
            'base/android/media_decoder_job.h',
            'base/android/media_drm_bridge.cc',
            'base/android/media_drm_bridge.h',
            'base/android/media_drm_bridge_cdm_context.cc',
            'base/android/media_drm_bridge_cdm_context.h',
            'base/android/media_drm_bridge_delegate.cc',
            'base/android/media_drm_bridge_delegate.h',
            'base/android/media_jni_registrar.cc',
            'base/android/media_jni_registrar.h',
            'base/android/media_player_android.cc',
            'base/android/media_player_android.h',
            'base/android/media_player_bridge.cc',
            'base/android/media_player_bridge.h',
            'base/android/media_player_listener.cc',
            'base/android/media_player_listener.h',
            'base/android/media_player_manager.h',
            'base/android/media_resource_getter.cc',
            'base/android/media_resource_getter.h',
            'base/android/media_source_player.cc',
            'base/android/media_source_player.h',
            'base/android/media_statistics.cc',
            'base/android/media_statistics.h',
            'base/android/media_task_runner.cc',
            'base/android/media_task_runner.h',
            'base/android/media_url_interceptor.h',
            'base/android/provision_fetcher.h',
            'base/android/sdk_media_codec_bridge.cc',
            'base/android/sdk_media_codec_bridge.h',
            'base/android/video_decoder_job.cc',
            'base/android/video_decoder_job.h',
            'base/android/video_media_codec_decoder.cc',
            'base/android/video_media_codec_decoder.h',
          ],
          'conditions': [
            # Only 64 bit builds are using android-21 NDK library, check common.gypi
            ['target_arch=="arm64" or target_arch=="x64" or target_arch=="mips64el"', {
              'sources': [
                'base/android/ndk_media_codec_bridge.cc',
                'base/android/ndk_media_codec_bridge.h',
                'base/android/ndk_media_codec_wrapper.cc',
              ],
            }],
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../third_party/widevine/cdm/widevine_cdm.gyp:widevine_cdm_version_h',
            '../ui/gl/gl.gyp:gl',
            '../url/url.gyp:url_lib',
            'media_android_jni_headers',
            'shared_memory_support',
          ],
          'include_dirs': [
            # Needed by media_drm_bridge.cc.
            '<(SHARED_INTERMEDIATE_DIR)',
          ],
          'defines': [
            'MEDIA_IMPLEMENTATION',
          ],
        },
        {
          # GN: //media/base/android:media_java
          'target_name': 'media_java',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base',
          ],
          'export_dependent_settings': [
            '../base/base.gyp:base',
          ],
          'variables': {
            'java_in_dir': 'base/android/java',
          },
          'includes': ['../build/java.gypi'],
        },
      ],
      'conditions': [
        ['test_isolation_mode != "noop"',
          {
            'targets': [
              {
                'target_name': 'media_unittests_apk_run',
                'type': 'none',
                'dependencies': [
                  'media_unittests_apk',
                ],
                'includes': [
                  '../build/isolate.gypi',
                ],
                'sources': [
                  'media_unittests_apk.isolate',
                ],
              },
            ],
          },
        ],
      ],
    }],
    # TODO(watk): Refactor tests that could be made to run on Android. See
    # http://crbug.com/570762
    ['media_use_ffmpeg==1 and OS!="android"', {
      'targets': [
        {
          # GN version: //media:ffmpeg_regression_tests
          'target_name': 'ffmpeg_regression_tests',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:test_support_base',
            '../testing/gmock.gyp:gmock',
            '../testing/gtest.gyp:gtest',
            '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            'media',
            'media_test_support',
          ],
          'sources': [
            'base/run_all_unittests.cc',
            'ffmpeg/ffmpeg_regression_tests.cc',
            'test/pipeline_integration_test_base.cc',
          ],
        },
      ],
    }],
    ['OS=="ios"', {
      'targets': [
        {
          # Minimal media component for media/cast on iOS.
          # GN version: //media:media_for_cast_ios
          'target_name': 'media_for_cast_ios',
          'type': '<(component)',
          'dependencies': [
            '../base/base.gyp:base',
            '../gpu/gpu.gyp:command_buffer_common',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            'shared_memory_support',
          ],
          'defines': [
            'MEDIA_DISABLE_FFMPEG',
            'MEDIA_DISABLE_LIBVPX',
            'MEDIA_FOR_CAST_IOS',
            'MEDIA_IMPLEMENTATION',
          ],
          'direct_dependent_settings': {
            'defines': [
              'MEDIA_DISABLE_FFMPEG',
              'MEDIA_DISABLE_LIBVPX',
            ],
            'include_dirs': [
              '..',
            ],
          },
          'include_dirs': [
            '..',
          ],
          'sources': [
            'base/mac/coremedia_glue.h',
            'base/mac/coremedia_glue.mm',
            'base/mac/corevideo_glue.h',
            'base/mac/video_frame_mac.cc',
            'base/mac/video_frame_mac.h',
            'base/mac/videotoolbox_glue.h',
            'base/mac/videotoolbox_glue.mm',
            'base/mac/videotoolbox_helpers.cc',
            'base/mac/videotoolbox_helpers.h',
            'base/simd/convert_rgb_to_yuv.h',
            'base/simd/convert_rgb_to_yuv_c.cc',
            'base/simd/convert_yuv_to_rgb.h',
            'base/simd/convert_yuv_to_rgb_c.cc',
            'base/simd/filter_yuv.h',
            'base/simd/filter_yuv_c.cc',
            'base/video_frame.cc',
            'base/video_frame.h',
            'base/video_frame_metadata.cc',
            'base/video_frame_metadata.h',
            'base/video_types.cc',
            'base/video_types.h',
            'base/video_util.cc',
            'base/video_util.h',
            'base/yuv_convert.cc',
            'base/yuv_convert.h',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/CoreVideo.framework',
            ],
          },
          'conditions': [
            ['arm_neon==1', {
              'defines': [
                'USE_NEON'
              ],
            }],
          ],  # conditions
          'target_conditions': [
            ['OS == "ios" and _toolset != "host"', {
              'sources/': [
                # Pull in specific Mac files for iOS (which have been filtered
                # out by file name rules).
                ['include', '^base/mac/coremedia_glue\\.h$'],
                ['include', '^base/mac/coremedia_glue\\.mm$'],
                ['include', '^base/mac/corevideo_glue\\.h$'],
                ['include', '^base/mac/videotoolbox_glue\\.h$'],
                ['include', '^base/mac/videotoolbox_glue\\.mm$'],
                ['include', '^base/mac/video_frame_mac\\.h$'],
                ['include', '^base/mac/video_frame_mac\\.cc$'],
              ],
            }],
          ],  # target_conditions
        },
      ],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'media_unittests_run',
          'type': 'none',
          'dependencies': [
            'media_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'media_unittests.isolate',
          ],
        },
        {
          'target_name': 'audio_unittests_run',
          'type': 'none',
          'dependencies': [
            'audio_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'audio_unittests.isolate',
          ],
          'conditions': [
            ['use_x11==1', {
              'dependencies': [
                '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
              ],
            }],
          ],
        },
      ],
    }],
    ['chromeos==1', {
      'targets': [
        {
          'target_name': 'jpeg_decode_accelerator_unittest',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            '../media/media.gyp:media',
            '../media/media.gyp:media_gpu',
            '../media/media.gyp:media_test_support',
            '../testing/gtest.gyp:gtest',
            '../third_party/libyuv/libyuv.gyp:libyuv',
            '../ui/gfx/gfx.gyp:gfx',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            '../ui/gl/gl.gyp:gl',
            '../ui/gl/gl.gyp:gl_test_support',
          ],
          'sources': [
            'gpu/jpeg_decode_accelerator_unittest.cc',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/libva',
            '<(DEPTH)/third_party/libyuv',
          ],
        }
      ]
    }],
    ['chromeos==1 or OS=="mac"', {
      'targets': [
        {
          'target_name': 'video_encode_accelerator_unittest',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            '../media/media.gyp:media',
            '../media/media.gyp:media_gpu',
            '../media/media.gyp:media_test_support',
            '../testing/gtest.gyp:gtest',
            '../ui/base/ui_base.gyp:ui_base',
            '../ui/gfx/gfx.gyp:gfx',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            '../ui/gfx/gfx.gyp:gfx_test_support',
            '../ui/gl/gl.gyp:gl',
            '../ui/gl/gl.gyp:gl_test_support',
          ],
          'sources': [
            'gpu/video_accelerator_unittest_helpers.h',
            'gpu/video_encode_accelerator_unittest.cc',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/libva',
            '<(DEPTH)/third_party/libyuv',
          ],
          'conditions': [
            ['OS=="mac"', {
              'dependencies': [
                '../third_party/webrtc/common_video/common_video.gyp:common_video',
              ],
            }],
            ['use_x11==1', {
              'dependencies': [
                '../ui/gfx/x/gfx_x11.gyp:gfx_x11',
              ],
            }],
            ['use_ozone==1', {
              'dependencies': [
                '../ui/ozone/ozone.gyp:ozone',
              ],
            }],
          ],
        }
      ]
    }],
    ['chromeos==1 or OS=="win" or OS=="android"', {
      'targets': [
          {
            # GN: //media/gpu:video_decode_accelerator_unittest
            'target_name': 'video_decode_accelerator_unittest',
            'type': '<(gtest_target_type)',
            'dependencies': [
              '../base/base.gyp:base',
              '../gpu/gpu.gyp:command_buffer_service',
              '../media/gpu/ipc/media_ipc.gyp:media_gpu_ipc_service',
              '../media/media.gyp:media',
              '../media/media.gyp:media_gpu',
              '../testing/gtest.gyp:gtest',
              '../ui/base/ui_base.gyp:ui_base',
              '../ui/gfx/gfx.gyp:gfx',
              '../ui/gfx/gfx.gyp:gfx_geometry',
              '../ui/gfx/gfx.gyp:gfx_test_support',
              '../ui/gl/gl.gyp:gl',
              '../ui/gl/gl.gyp:gl_test_support',
            ],
            'include_dirs': [
              '<(DEPTH)/third_party/khronos',
            ],
            'sources': [
              'gpu/android_video_decode_accelerator_unittest.cc',
              'gpu/rendering_helper.cc',
              'gpu/rendering_helper.h',
              'gpu/video_accelerator_unittest_helpers.h',
              'gpu/video_decode_accelerator_unittest.cc',
            ],
            'conditions': [
              ['OS=="android"', {
                'sources/': [
                  ['exclude', '^gpu/rendering_helper.h'],
                  ['exclude', '^gpu/rendering_helper.cc'],
                  ['exclude', '^gpu/video_decode_accelerator_unittest.cc'],
                ],
                'dependencies': [
                  '../media/media.gyp:player_android',
                  '../testing/gmock.gyp:gmock',
                  '../testing/android/native_test.gyp:native_test_native_code',
                  '../gpu/gpu.gyp:gpu_unittest_utils',
                ],
              }, {  # OS!="android"
                'sources/': [
                  ['exclude', '^gpu/android_video_decode_accelerator_unittest.cc'],
                ],
              }],
              ['OS=="win"', {
                'dependencies': [
                  '<(angle_path)/src/angle.gyp:libEGL',
                  '<(angle_path)/src/angle.gyp:libGLESv2',
                ],
              }],
              ['target_arch != "arm" and (OS=="linux" or chromeos == 1)', {
                'include_dirs': [
                  '<(DEPTH)/third_party/libva',
                ],
              }],
              ['use_x11==1', {
                'dependencies': [
                  '../build/linux/system.gyp:x11',  # Used by rendering_helper.cc
                  '../ui/gfx/x/gfx_x11.gyp:gfx_x11',
                ],
              }],
              ['use_ozone==1 and chromeos==1', {
                'dependencies': [
                  '../ui/display/display.gyp:display',  # Used by rendering_helper.cc
                  '../ui/ozone/ozone.gyp:ozone',  # Used by rendering_helper.cc
                ],
              }],
            ],
            # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
            'msvs_disabled_warnings': [ 4267 ],
          },
        ],
    }],
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'video_decode_accelerator_unittest_apk',
          'type': 'none',
          'dependencies': [
            'video_decode_accelerator_unittest',
          ],
          'variables': {
            'test_suite_name': 'video_decode_accelerator_unittest',
          },
          'includes': [ '../build/apk_test.gypi' ],
        },
      ],
    }],

    ['chromeos==1 and target_arch != "arm"', {
      'targets': [
          {
            'target_name': 'vaapi_jpeg_decoder_unittest',
            'type': '<(gtest_target_type)',
            'dependencies': [
              '../media/media.gyp:media_gpu',
              '../base/base.gyp:base',
              '../media/media.gyp:media',
              '../media/media.gyp:media_test_support',
              '../testing/gtest.gyp:gtest',
            ],
            'sources': [
              'gpu/vaapi_jpeg_decoder_unittest.cc',
            ],
            'include_dirs': [
              '<(DEPTH)/third_party/libva',
            ],
            'conditions': [
              ['use_x11==1', {
                'dependencies': [
                  '../build/linux/system.gyp:x11',
                  '../ui/gfx/x/gfx_x11.gyp:gfx_x11',
                ]
              }, {
                'dependencies': [
                  '../build/linux/system.gyp:libdrm',
                ]
              }],
            ],
          }
        ]
    }],
  ],
}
