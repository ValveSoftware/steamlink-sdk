// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_DECODER_H_
#define MEDIA_BASE_AUDIO_DECODER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"

namespace media {

class AudioBuffer;
class DemuxerStream;

class MEDIA_EXPORT AudioDecoder {
 public:
  // Status codes for decode operations.
  // TODO(rileya): Now that both AudioDecoder and VideoDecoder Status enums
  // match, break them into a decoder_status.h.
  enum Status {
    kOk,  // We're all good.
    kAborted,  // We aborted as a result of Stop() or Reset().
    kDecodeError,  // A decoding error occurred.
    kDecryptError  // Decrypting error happened.
  };

  // Callback for AudioDecoder to return a decoded frame whenever it becomes
  // available. Only non-EOS frames should be returned via this callback.
  typedef base::Callback<void(const scoped_refptr<AudioBuffer>&)> OutputCB;

  // Callback for Decode(). Called after the decoder has completed decoding
  // corresponding DecoderBuffer, indicating that it's ready to accept another
  // buffer to decode.
  typedef base::Callback<void(Status)> DecodeCB;

  AudioDecoder();
  virtual ~AudioDecoder();

  // Initializes an AudioDecoder with the given DemuxerStream, executing the
  // callback upon completion.
  //  |statistics_cb| is used to update global pipeline statistics.
  //  |output_cb| is called for decoded audio buffers (see Decode()).
  virtual void Initialize(const AudioDecoderConfig& config,
                          const PipelineStatusCB& status_cb,
                          const OutputCB& output_cb) = 0;

  // Requests samples to be decoded. Only one decode may be in flight at any
  // given time. Once the buffer is decoded the decoder calls |decode_cb|.
  // |output_cb| specified in Initialize() is called for each decoded buffer,
  // before or after |decode_cb|.
  //
  // Implementations guarantee that the callbacks will not be called from within
  // this method.
  //
  // If |buffer| is an EOS buffer then the decoder must be flushed, i.e.
  // |output_cb| must be called for each frame pending in the queue and
  // |decode_cb| must be called after that.
  virtual void Decode(const scoped_refptr<DecoderBuffer>& buffer,
                      const DecodeCB& decode_cb) = 0;

  // Resets decoder state. All pending Decode() requests will be finished or
  // aborted before |closure| is called.
  virtual void Reset(const base::Closure& closure) = 0;

  // Stops decoder, fires any pending callbacks and sets the decoder to an
  // uninitialized state. An AudioDecoder cannot be re-initialized after it has
  // been stopped. DecodeCB and OutputCB may still be called for older buffers
  // if they were scheduled before this method is called.
  // Note that if Initialize() is pending or has finished successfully, Stop()
  // must be called before destructing the decoder.
  virtual void Stop() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioDecoder);
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_DECODER_H_
