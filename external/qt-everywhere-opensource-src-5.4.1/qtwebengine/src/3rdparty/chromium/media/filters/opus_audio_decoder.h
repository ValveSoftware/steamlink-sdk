// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_OPUS_AUDIO_DECODER_H_
#define MEDIA_FILTERS_OPUS_AUDIO_DECODER_H_

#include "base/callback.h"
#include "base/time/time.h"
#include "media/base/audio_decoder.h"
#include "media/base/demuxer_stream.h"
#include "media/base/sample_format.h"

struct OpusMSDecoder;

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class AudioBuffer;
class AudioDiscardHelper;
class DecoderBuffer;
struct QueuedAudioBuffer;

class MEDIA_EXPORT OpusAudioDecoder : public AudioDecoder {
 public:
  explicit OpusAudioDecoder(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  virtual ~OpusAudioDecoder();

  // AudioDecoder implementation.
  virtual void Initialize(const AudioDecoderConfig& config,
                          const PipelineStatusCB& status_cb,
                          const OutputCB& output_cb) OVERRIDE;
  virtual void Decode(const scoped_refptr<DecoderBuffer>& buffer,
                      const DecodeCB& decode_cb) OVERRIDE;
  virtual void Reset(const base::Closure& closure) OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  // Reads from the demuxer stream with corresponding callback method.
  void ReadFromDemuxerStream();
  void DecodeBuffer(const scoped_refptr<DecoderBuffer>& input,
                    const DecodeCB& decode_cb);

  bool ConfigureDecoder();
  void CloseDecoder();
  void ResetTimestampState();
  bool Decode(const scoped_refptr<DecoderBuffer>& input,
              scoped_refptr<AudioBuffer>* output_buffer);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  AudioDecoderConfig config_;
  OutputCB output_cb_;
  OpusMSDecoder* opus_decoder_;

  // When the input timestamp is |start_input_timestamp_| the decoder needs to
  // drop |config_.codec_delay()| frames.
  base::TimeDelta start_input_timestamp_;

  scoped_ptr<AudioDiscardHelper> discard_helper_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(OpusAudioDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_OPUS_AUDIO_DECODER_H_
