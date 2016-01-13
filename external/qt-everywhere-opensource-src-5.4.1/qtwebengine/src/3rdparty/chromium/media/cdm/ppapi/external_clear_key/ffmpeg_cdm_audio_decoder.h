// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_FFMPEG_CDM_AUDIO_DECODER_H_
#define MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_FFMPEG_CDM_AUDIO_DECODER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "media/cdm/ppapi/external_clear_key/clear_key_cdm_common.h"
#include "media/ffmpeg/ffmpeg_deleters.h"

struct AVCodecContext;
struct AVFrame;

namespace media {
class AudioBus;
class AudioTimestampHelper;
}

namespace media {
// TODO(xhwang): This class is partially cloned from FFmpegAudioDecoder. When
// FFmpegAudioDecoder is updated, it's a pain to keep this class in sync with
// FFmpegAudioDecoder. We need a long term sustainable solution for this. See
// http://crbug.com/169203
class FFmpegCdmAudioDecoder {
 public:
  explicit FFmpegCdmAudioDecoder(ClearKeyCdmHost* host);
  ~FFmpegCdmAudioDecoder();
  bool Initialize(const cdm::AudioDecoderConfig& config);
  void Deinitialize();
  void Reset();

  // Returns true when |config| is a valid audio decoder configuration.
  static bool IsValidConfig(const cdm::AudioDecoderConfig& config);

  // Decodes |compressed_buffer|. Returns |cdm::kSuccess| after storing
  // output in |decoded_frames| when output is available. Returns
  // |cdm::kNeedMoreData| when |compressed_frame| does not produce output.
  // Returns |cdm::kDecodeError| when decoding fails.
  //
  // A null |compressed_buffer| will attempt to flush the decoder of any
  // remaining frames. |compressed_buffer_size| and |timestamp| are ignored.
  cdm::Status DecodeBuffer(const uint8_t* compressed_buffer,
                           int32_t compressed_buffer_size,
                           int64_t timestamp,
                           cdm::AudioFrames* decoded_frames);

 private:
  void ResetTimestampState();
  void ReleaseFFmpegResources();

  base::TimeDelta GetNextOutputTimestamp() const;

  void SerializeInt64(int64_t value);

  bool is_initialized_;

  ClearKeyCdmHost* const host_;

  // FFmpeg structures owned by this object.
  scoped_ptr<AVCodecContext, ScopedPtrAVFreeContext> codec_context_;
  scoped_ptr<AVFrame, ScopedPtrAVFreeFrame> av_frame_;

  // Audio format.
  int samples_per_second_;
  int channels_;

  // AVSampleFormat initially requested; not Chrome's SampleFormat.
  int av_sample_format_;

  // Used for computing output timestamps.
  scoped_ptr<AudioTimestampHelper> output_timestamp_helper_;
  int bytes_per_frame_;
  base::TimeDelta last_input_timestamp_;

  // Number of output sample bytes to drop before generating output buffers.
  // This is required for handling negative timestamps when decoding Vorbis
  // audio, for example.
  int output_bytes_to_drop_;

  typedef std::vector<uint8_t> SerializedAudioFrames;
  SerializedAudioFrames serialized_audio_frames_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegCdmAudioDecoder);
};

}  // namespace media

#endif  // MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_FFMPEG_CDM_AUDIO_DECODER_H_
