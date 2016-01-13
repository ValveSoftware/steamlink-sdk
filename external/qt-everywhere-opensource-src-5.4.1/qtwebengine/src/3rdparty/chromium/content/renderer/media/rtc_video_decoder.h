// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_VIDEO_DECODER_H_
#define CONTENT_RENDERER_MEDIA_RTC_VIDEO_DECODER_H_

#include <deque>
#include <list>
#include <map>
#include <set>
#include <utility>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/video_decoder.h"
#include "media/video/picture.h"
#include "media/video/video_decode_accelerator.h"
#include "third_party/webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"

namespace base {
class WaitableEvent;
class MessageLoopProxy;
};

namespace media {
class DecoderBuffer;
class GpuVideoAcceleratorFactories;
}

namespace content {

// This class uses hardware accelerated video decoder to decode video for
// WebRTC. |vda_message_loop_| is the message loop proxy of the media thread,
// which VDA::Client methods run on. webrtc::VideoDecoder methods run on WebRTC
// DecodingThread or Chrome_libJingle_WorkerThread, which are trampolined to
// |vda_message_loop_|. Decode() is non-blocking and queues the buffers. Decoded
// frames are delivered to WebRTC on |vda_message_loop_|.
class CONTENT_EXPORT RTCVideoDecoder
    : NON_EXPORTED_BASE(public webrtc::VideoDecoder),
      public media::VideoDecodeAccelerator::Client {
 public:
  virtual ~RTCVideoDecoder();

  // Creates a RTCVideoDecoder. Returns NULL if failed. The video decoder will
  // run on the message loop of |factories|.
  static scoped_ptr<RTCVideoDecoder> Create(
      webrtc::VideoCodecType type,
      const scoped_refptr<media::GpuVideoAcceleratorFactories>& factories);

  // webrtc::VideoDecoder implementation.
  // Called on WebRTC DecodingThread.
  virtual int32_t InitDecode(const webrtc::VideoCodec* codecSettings,
                             int32_t numberOfCores) OVERRIDE;
  // Called on WebRTC DecodingThread.
  virtual int32_t Decode(
      const webrtc::EncodedImage& inputImage,
      bool missingFrames,
      const webrtc::RTPFragmentationHeader* fragmentation,
      const webrtc::CodecSpecificInfo* codecSpecificInfo = NULL,
      int64_t renderTimeMs = -1) OVERRIDE;
  // Called on WebRTC DecodingThread.
  virtual int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* callback) OVERRIDE;
  // Called on Chrome_libJingle_WorkerThread. The child thread is blocked while
  // this runs.
  virtual int32_t Release() OVERRIDE;
  // Called on Chrome_libJingle_WorkerThread. The child thread is blocked while
  // this runs.
  virtual int32_t Reset() OVERRIDE;

  // VideoDecodeAccelerator::Client implementation.
  virtual void ProvidePictureBuffers(uint32 count,
                                     const gfx::Size& size,
                                     uint32 texture_target) OVERRIDE;
  virtual void DismissPictureBuffer(int32 id) OVERRIDE;
  virtual void PictureReady(const media::Picture& picture) OVERRIDE;
  virtual void NotifyEndOfBitstreamBuffer(int32 id) OVERRIDE;
  virtual void NotifyFlushDone() OVERRIDE;
  virtual void NotifyResetDone() OVERRIDE;
  virtual void NotifyError(media::VideoDecodeAccelerator::Error error) OVERRIDE;

 private:
  class SHMBuffer;
  // Metadata of a bitstream buffer.
  struct BufferData {
    BufferData(int32 bitstream_buffer_id,
               uint32_t timestamp,
               int width,
               int height,
               size_t size);
    BufferData();
    ~BufferData();
    int32 bitstream_buffer_id;
    uint32_t timestamp;  // in 90KHz
    uint32_t width;
    uint32_t height;
    size_t size;  // buffer size
  };

  FRIEND_TEST_ALL_PREFIXES(RTCVideoDecoderTest, IsBufferAfterReset);
  FRIEND_TEST_ALL_PREFIXES(RTCVideoDecoderTest, IsFirstBufferAfterReset);

  RTCVideoDecoder(
      const scoped_refptr<media::GpuVideoAcceleratorFactories>& factories);

  // Requests a buffer to be decoded by VDA.
  void RequestBufferDecode();

  bool CanMoreDecodeWorkBeDone();

  // Returns true if bitstream buffer id |id_buffer| comes after |id_reset|.
  // This handles the wraparound.
  bool IsBufferAfterReset(int32 id_buffer, int32 id_reset);

  // Returns true if bitstream buffer |id_buffer| is the first buffer after
  // |id_reset|.
  bool IsFirstBufferAfterReset(int32 id_buffer, int32 id_reset);

  // Saves a WebRTC buffer in |decode_buffers_| for decode.
  void SaveToDecodeBuffers_Locked(const webrtc::EncodedImage& input_image,
                                  scoped_ptr<SHMBuffer> shm_buffer,
                                  const BufferData& buffer_data);

  // Saves a WebRTC buffer in |pending_buffers_| waiting for SHM available.
  // Returns true on success.
  bool SaveToPendingBuffers_Locked(const webrtc::EncodedImage& input_image,
                                   const BufferData& buffer_data);

  // Gets SHM and moves pending buffers to decode buffers.
  void MovePendingBuffersToDecodeBuffers();

  scoped_refptr<media::VideoFrame> CreateVideoFrame(
      const media::Picture& picture,
      const media::PictureBuffer& pb,
      uint32_t timestamp,
      uint32_t width,
      uint32_t height,
      size_t size);

  // Resets VDA.
  void ResetInternal();

  // Static method is to allow it to run even after RVD is deleted.
  static void ReleaseMailbox(
      base::WeakPtr<RTCVideoDecoder> decoder,
      const scoped_refptr<media::GpuVideoAcceleratorFactories>& factories,
      int64 picture_buffer_id,
      uint32 texture_id,
      const std::vector<uint32>& release_sync_points);
  // Tells VDA that a picture buffer can be recycled.
  void ReusePictureBuffer(int64 picture_buffer_id);

  // Create |vda_| on |vda_loop_proxy_|.
  void CreateVDA(media::VideoCodecProfile profile, base::WaitableEvent* waiter);

  void DestroyTextures();
  void DestroyVDA();

  // Gets a shared-memory segment of at least |min_size| bytes from
  // |available_shm_segments_|. Returns NULL if there is no buffer or the
  // buffer is not big enough.
  scoped_ptr<SHMBuffer> GetSHM_Locked(size_t min_size);

  // Returns a shared-memory segment to the available pool.
  void PutSHM_Locked(scoped_ptr<SHMBuffer> shm_buffer);

  // Allocates |number| shared memory of at least |min_size| bytes.
  void CreateSHM(int number, size_t min_size);

  // Stores the buffer metadata to |input_buffer_data_|.
  void RecordBufferData(const BufferData& buffer_data);
  // Gets the buffer metadata from |input_buffer_data_|.
  void GetBufferData(int32 bitstream_buffer_id,
                     uint32_t* timestamp,
                     uint32_t* width,
                     uint32_t* height,
                     size_t* size);

  // Records the result of InitDecode to UMA and returns |status|.
  int32_t RecordInitDecodeUMA(int32_t status);

  // Assert the contract that this class is operated on the right thread.
  void DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent() const;

  enum State {
    UNINITIALIZED,  // The decoder has not initialized.
    INITIALIZED,    // The decoder has initialized.
    RESETTING,      // The decoder is being reset.
    DECODE_ERROR,   // Decoding error happened.
  };

  static const int32 ID_LAST;     // maximum bitstream buffer id
  static const int32 ID_HALF;     // half of the maximum bitstream buffer id
  static const int32 ID_INVALID;  // indicates Reset or Release never occurred

  // The hardware video decoder.
  scoped_ptr<media::VideoDecodeAccelerator> vda_;

  // The size of the incoming video frames.
  gfx::Size frame_size_;

  scoped_refptr<media::GpuVideoAcceleratorFactories> factories_;

  // The texture target used for decoded pictures.
  uint32 decoder_texture_target_;

  // Metadata of the buffers that have been sent for decode.
  std::list<BufferData> input_buffer_data_;

  // A map from bitstream buffer IDs to bitstream buffers that are being
  // processed by VDA. The map owns SHM buffers.
  std::map<int32, SHMBuffer*> bitstream_buffers_in_decoder_;

  // A map from picture buffer IDs to texture-backed picture buffers.
  std::map<int32, media::PictureBuffer> assigned_picture_buffers_;

  // PictureBuffers given to us by VDA via PictureReady, which we sent forward
  // as VideoFrames to be rendered via read_cb_, and which will be returned
  // to us via ReusePictureBuffer.
  typedef std::map<int32 /* picture_buffer_id */, uint32 /* texture_id */>
      PictureBufferTextureMap;
  PictureBufferTextureMap picture_buffers_at_display_;

  // The id that will be given to the next picture buffer.
  int32 next_picture_buffer_id_;

  // Protects |state_|, |decode_complete_callback_| , |num_shm_buffers_|,
  // |available_shm_segments_|, |pending_buffers_|, |decode_buffers_|,
  // |next_bitstream_buffer_id_| and |reset_bitstream_buffer_id_|.
  base::Lock lock_;

  // The state of RTCVideoDecoder. Guarded by |lock_|.
  State state_;

  // Guarded by |lock_|.
  webrtc::DecodedImageCallback* decode_complete_callback_;

  // Total number of allocated SHM buffers. Guarded by |lock_|.
  int num_shm_buffers_;

  // Shared-memory buffer pool.  Since allocating SHM segments requires a
  // round-trip to the browser process, we keep allocation out of the
  // steady-state of the decoder. The vector owns SHM buffers. Guarded by
  // |lock_|.
  std::vector<SHMBuffer*> available_shm_segments_;

  // A queue storing WebRTC encoding images (and their metadata) that are
  // waiting for the shared memory. Guarded by |lock_|.
  std::deque<std::pair<webrtc::EncodedImage, BufferData> > pending_buffers_;

  // A queue storing buffers (and their metadata) that will be sent to VDA for
  // decode. The queue owns SHM buffers. Guarded by |lock_|.
  std::deque<std::pair<SHMBuffer*, BufferData> > decode_buffers_;

  // The id that will be given to the next bitstream buffer. Guarded by |lock_|.
  int32 next_bitstream_buffer_id_;

  // A buffer that has an id less than this should be dropped because Reset or
  // Release has been called. Guarded by |lock_|.
  int32 reset_bitstream_buffer_id_;

  // Must be destroyed, or invalidated, on |vda_loop_proxy_|
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<RTCVideoDecoder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RTCVideoDecoder);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RTC_VIDEO_DECODER_H_
