// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_ANDROID_VIDEO_DECODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_ANDROID_VIDEO_DECODE_ACCELERATOR_H_

#include <list>
#include <map>
#include <queue>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "gpu/command_buffer/service/gles2_cmd_copy_texture_chromium.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/video/video_decode_accelerator.h"

namespace gfx {
class SurfaceTexture;
}

namespace content {
// A VideoDecodeAccelerator implementation for Android.
// This class decodes the input encoded stream by using Android's MediaCodec
// class. http://developer.android.com/reference/android/media/MediaCodec.html
class CONTENT_EXPORT AndroidVideoDecodeAccelerator
    : public media::VideoDecodeAccelerator {
 public:
  // Does not take ownership of |client| which must outlive |*this|.
  AndroidVideoDecodeAccelerator(
      const base::WeakPtr<gpu::gles2::GLES2Decoder> decoder,
      const base::Callback<bool(void)>& make_context_current);

  // media::VideoDecodeAccelerator implementation.
  virtual bool Initialize(media::VideoCodecProfile profile,
                          Client* client) OVERRIDE;
  virtual void Decode(const media::BitstreamBuffer& bitstream_buffer) OVERRIDE;
  virtual void AssignPictureBuffers(
      const std::vector<media::PictureBuffer>& buffers) OVERRIDE;
  virtual void ReusePictureBuffer(int32 picture_buffer_id) OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool CanDecodeOnIOThread() OVERRIDE;

 private:
  enum State {
    NO_ERROR,
    ERROR,
  };

  static const base::TimeDelta kDecodePollDelay;

  virtual ~AndroidVideoDecodeAccelerator();

  // Configures |media_codec_| with the given codec parameters from the client.
  bool ConfigureMediaCodec();

  // Sends the current picture on the surface to the client.
  void SendCurrentSurfaceToClient(int32 bitstream_id);

  // Does pending IO tasks if any. Once this is called, it polls |media_codec_|
  // until it finishes pending tasks. For the polling, |kDecodePollDelay| is
  // used.
  void DoIOTask();

  // Feeds input data to |media_codec_|. This checks
  // |pending_bitstream_buffers_| and queues a buffer to |media_codec_|.
  void QueueInput();

  // Dequeues output from |media_codec_| and feeds the decoded frame to the
  // client.
  void DequeueOutput();

  // Requests picture buffers from the client.
  void RequestPictureBuffers();

  // Notifies the client about the availability of a picture.
  void NotifyPictureReady(const media::Picture& picture);

  // Notifies the client that the input buffer identifed by input_buffer_id has
  // been processed.
  void NotifyEndOfBitstreamBuffer(int input_buffer_id);

  // Notifies the client that the decoder was flushed.
  void NotifyFlushDone();

  // Notifies the client that the decoder was reset.
  void NotifyResetDone();

  // Notifies about decoding errors.
  void NotifyError(media::VideoDecodeAccelerator::Error error);

  // Used to DCHECK that we are called on the correct thread.
  base::ThreadChecker thread_checker_;

  // To expose client callbacks from VideoDecodeAccelerator.
  Client* client_;

  // Callback to set the correct gl context.
  base::Callback<bool(void)> make_context_current_;

  // Codec type. Used when we configure media codec.
  media::VideoCodec codec_;

  // The current state of this class. For now, this is used only for setting
  // error state.
  State state_;

  // This map maintains the picture buffers passed to the client for decoding.
  // The key is the picture buffer id.
  typedef std::map<int32, media::PictureBuffer> OutputBufferMap;
  OutputBufferMap output_picture_buffers_;

  // This keeps the free picture buffer ids which can be used for sending
  // decoded frames to the client.
  std::queue<int32> free_picture_ids_;

  // Picture buffer ids which have been dismissed and not yet re-assigned.  Used
  // to ignore ReusePictureBuffer calls that were in flight when the
  // DismissPictureBuffer call was made.
  std::set<int32> dismissed_picture_ids_;

  // The low-level decoder which Android SDK provides.
  scoped_ptr<media::VideoCodecBridge> media_codec_;

  // A container of texture. Used to set a texture to |media_codec_|.
  scoped_refptr<gfx::SurfaceTexture> surface_texture_;

  // The texture id which is set to |surface_texture_|.
  uint32 surface_texture_id_;

  // Set to true after requesting picture buffers to the client.
  bool picturebuffers_requested_;

  // The resolution of the stream.
  gfx::Size size_;

  // Encoded bitstream buffers to be passed to media codec, queued until an
  // input buffer is available, along with the time when they were first
  // enqueued.
  typedef std::queue<std::pair<media::BitstreamBuffer, base::Time> >
      PendingBitstreamBuffers;
  PendingBitstreamBuffers pending_bitstream_buffers_;

  // Keeps track of bitstream ids notified to the client with
  // NotifyEndOfBitstreamBuffer() before getting output from the bitstream.
  std::list<int32> bitstreams_notified_in_advance_;

  // Owner of the GL context. Used to restore the context state.
  base::WeakPtr<gpu::gles2::GLES2Decoder> gl_decoder_;

  // Used for copy the texture from |surface_texture_| to picture buffers.
  scoped_ptr<gpu::CopyTextureCHROMIUMResourceManager> copier_;

  // Repeating timer responsible for draining pending IO to the codec.
  base::RepeatingTimer<AndroidVideoDecodeAccelerator> io_timer_;

  // WeakPtrFactory for posting tasks back to |this|.
  base::WeakPtrFactory<AndroidVideoDecodeAccelerator> weak_this_factory_;

  friend class AndroidVideoDecodeAcceleratorTest;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_ANDROID_VIDEO_DECODE_ACCELERATOR_H_
