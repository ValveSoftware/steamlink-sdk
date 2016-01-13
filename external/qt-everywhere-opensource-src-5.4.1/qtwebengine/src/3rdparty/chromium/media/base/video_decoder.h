// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_DECODER_H_
#define MEDIA_BASE_VIDEO_DECODER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"
#include "ui/gfx/size.h"

namespace media {

class DecoderBuffer;
class VideoDecoderConfig;
class VideoFrame;

class MEDIA_EXPORT VideoDecoder {
 public:
  // Status codes for decode operations on VideoDecoder.
  // TODO(rileya): Now that both AudioDecoder and VideoDecoder Status enums
  // match, break them into a decoder_status.h.
  enum Status {
    kOk,  // Everything went as planned.
    kAborted,  // Decode was aborted as a result of Reset() being called.
    kDecodeError,  // Decoding error happened.
    kDecryptError  // Decrypting error happened.
  };

  // Callback for VideoDecoder to return a decoded frame whenever it becomes
  // available. Only non-EOS frames should be returned via this callback.
  typedef base::Callback<void(const scoped_refptr<VideoFrame>&)> OutputCB;

  // Callback type for Decode(). Called after the decoder has completed decoding
  // corresponding DecoderBuffer, indicating that it's ready to accept another
  // buffer to decode.
  typedef base::Callback<void(Status status)> DecodeCB;

  VideoDecoder();
  virtual ~VideoDecoder();

  // Initializes a VideoDecoder with the given |config|, executing the
  // |status_cb| upon completion. |output_cb| is called for each output frame
  // decoded by Decode().
  //
  // Note:
  // 1) The VideoDecoder will be reinitialized if it was initialized before.
  //    Upon reinitialization, all internal buffered frames will be dropped.
  // 2) This method should not be called during pending decode, reset or stop.
  // 3) No VideoDecoder calls except for Stop() should be made before
  //    |status_cb| is executed.
  virtual void Initialize(const VideoDecoderConfig& config,
                          bool low_delay,
                          const PipelineStatusCB& status_cb,
                          const OutputCB& output_cb) = 0;

  // Requests a |buffer| to be decoded. The status of the decoder and decoded
  // frame are returned via the provided callback. Some decoders may allow
  // decoding multiple buffers in parallel. Callers should call
  // GetMaxDecodeRequests() to get number of buffers that may be decoded in
  // parallel. Decoder must call |decode_cb| in the same order in which Decode()
  // is called.
  //
  // Implementations guarantee that the callback will not be called from within
  // this method and that |decode_cb| will not be blocked on the following
  // Decode() calls (i.e. |decode_cb| will be called even Decode() is never
  // called again).
  //
  // After decoding is finished the decoder calls |output_cb| specified in
  // Initialize() for each decoded frame. |output_cb| may be called before or
  // after |decode_cb|.
  //
  // If |buffer| is an EOS buffer then the decoder must be flushed, i.e.
  // |output_cb| must be called for each frame pending in the queue and
  // |decode_cb| must be called after that.
  virtual void Decode(const scoped_refptr<DecoderBuffer>& buffer,
                      const DecodeCB& decode_cb) = 0;

  // Resets decoder state. All pending Decode() requests will be finished or
  // aborted before |closure| is called.
  // Note: No VideoDecoder calls should be made before |closure| is executed.
  virtual void Reset(const base::Closure& closure) = 0;

  // Stops decoder, fires any pending callbacks and sets the decoder to an
  // uninitialized state. A VideoDecoder cannot be re-initialized after it has
  // been stopped.
  // Note that if Initialize() is pending or has finished successfully, Stop()
  // must be called before destructing the decoder.
  virtual void Stop() = 0;

  // Returns true if the decoder needs bitstream conversion before decoding.
  virtual bool NeedsBitstreamConversion() const;

  // Returns true if the decoder currently has the ability to decode and return
  // a VideoFrame. Most implementations can allocate a new VideoFrame and hence
  // this will always return true. Override and return false for decoders that
  // use a fixed set of VideoFrames for decoding.
  virtual bool CanReadWithoutStalling() const;

  // Returns maximum number of parallel decode requests.
  virtual int GetMaxDecodeRequests() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoDecoder);
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_DECODER_H_
