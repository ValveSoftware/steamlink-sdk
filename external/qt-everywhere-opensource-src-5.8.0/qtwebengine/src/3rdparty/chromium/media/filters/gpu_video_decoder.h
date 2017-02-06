// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_GPU_VIDEO_DECODER_H_
#define MEDIA_FILTERS_GPU_VIDEO_DECODER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/base/pipeline_status.h"
#include "media/base/surface_manager.h"
#include "media/base/video_decoder.h"
#include "media/video/video_decode_accelerator.h"

template <class T> class scoped_refptr;

namespace base {
class SharedMemory;
class SingleThreadTaskRunner;
}

namespace gpu {
struct SyncToken;
}

namespace media {

class DecoderBuffer;
class GpuVideoAcceleratorFactories;
class MediaLog;

// GPU-accelerated video decoder implementation.  Relies on
// AcceleratedVideoDecoderMsg_Decode and friends.  Can be created on any thread
// but must be accessed and destroyed on GpuVideoAcceleratorFactories's
// GetMessageLoop().
class MEDIA_EXPORT GpuVideoDecoder
    : public VideoDecoder,
      public VideoDecodeAccelerator::Client {
 public:
  GpuVideoDecoder(GpuVideoAcceleratorFactories* factories,
                  const RequestSurfaceCB& request_surface_cb,
                  scoped_refptr<MediaLog> media_log);

  // VideoDecoder implementation.
  std::string GetDisplayName() const override;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  const InitCB& init_cb,
                  const OutputCB& output_cb) override;
  void CompleteInitialization(int cdm_id, int surface_id);
  void Decode(const scoped_refptr<DecoderBuffer>& buffer,
              const DecodeCB& decode_cb) override;
  void Reset(const base::Closure& closure) override;
  bool NeedsBitstreamConversion() const override;
  bool CanReadWithoutStalling() const override;
  int GetMaxDecodeRequests() const override;

  // VideoDecodeAccelerator::Client implementation.
  void NotifyInitializationComplete(bool success) override;
  void ProvidePictureBuffers(uint32_t count,
                             VideoPixelFormat format,
                             uint32_t textures_per_buffer,
                             const gfx::Size& size,
                             uint32_t texture_target) override;
  void DismissPictureBuffer(int32_t id) override;
  void PictureReady(const media::Picture& picture) override;
  void NotifyEndOfBitstreamBuffer(int32_t id) override;
  void NotifyFlushDone() override;
  void NotifyResetDone() override;
  void NotifyError(media::VideoDecodeAccelerator::Error error) override;

  static const char kDecoderName[];

 protected:
  ~GpuVideoDecoder() override;

 private:
  enum State {
    kNormal,
    kDrainingDecoder,
    kDecoderDrained,
    kError
  };

  // A shared memory segment and its allocated size.
  struct SHMBuffer {
    SHMBuffer(std::unique_ptr<base::SharedMemory> m, size_t s);
    ~SHMBuffer();
    std::unique_ptr<base::SharedMemory> shm;
    size_t size;
  };

  // A SHMBuffer and the DecoderBuffer its data came from.
  struct PendingDecoderBuffer {
    PendingDecoderBuffer(SHMBuffer* s,
                        const scoped_refptr<DecoderBuffer>& b,
                        const DecodeCB& done_cb);
    PendingDecoderBuffer(const PendingDecoderBuffer& other);
    ~PendingDecoderBuffer();
    SHMBuffer* shm_buffer;
    scoped_refptr<DecoderBuffer> buffer;
    DecodeCB done_cb;
  };

  typedef std::map<int32_t, PictureBuffer> PictureBufferMap;

  void DeliverFrame(const scoped_refptr<VideoFrame>& frame);

  // Static method is to allow it to run even after GVD is deleted.
  static void ReleaseMailbox(base::WeakPtr<GpuVideoDecoder> decoder,
                             media::GpuVideoAcceleratorFactories* factories,
                             int64_t picture_buffer_id,
                             PictureBuffer::TextureIds ids,
                             const gpu::SyncToken& release_sync_token);
  // Indicate the picture buffer can be reused by the decoder.
  void ReusePictureBuffer(int64_t picture_buffer_id);

  void RecordBufferData(
      const BitstreamBuffer& bitstream_buffer, const DecoderBuffer& buffer);
  void GetBufferData(int32_t id,
                     base::TimeDelta* timetamp,
                     gfx::Rect* visible_rect,
                     gfx::Size* natural_size);

  void DestroyVDA();

  // Request a shared-memory segment of at least |min_size| bytes.  Will
  // allocate as necessary.
  std::unique_ptr<SHMBuffer> GetSHM(size_t min_size);

  // Return a shared-memory segment to the available pool.
  void PutSHM(std::unique_ptr<SHMBuffer> shm_buffer);

  // Destroy all PictureBuffers in |buffers|, and delete their textures.
  void DestroyPictureBuffers(PictureBufferMap* buffers);

  // Returns true if the video decoder with |capabilities| can support
  // |profile|, |coded_size|, and |is_encrypted|.
  bool IsProfileSupported(
      const VideoDecodeAccelerator::Capabilities& capabilities,
      VideoCodecProfile profile,
      const gfx::Size& coded_size,
      bool is_encrypted);

  // Assert the contract that this class is operated on the right thread.
  void DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent() const;

  bool needs_bitstream_conversion_;

  GpuVideoAcceleratorFactories* factories_;

  // For requesting a suface to render to. If this is null the VDA will return
  // normal video frames and not render them to a surface.
  RequestSurfaceCB request_surface_cb_;

  scoped_refptr<MediaLog> media_log_;

  // Populated during Initialize() (on success) and unchanged until an error
  // occurs.
  std::unique_ptr<VideoDecodeAccelerator> vda_;

  InitCB init_cb_;
  OutputCB output_cb_;

  DecodeCB eos_decode_cb_;

  // Not null only during reset.
  base::Closure pending_reset_cb_;

  State state_;

  VideoDecoderConfig config_;

  // Shared-memory buffer pool.  Since allocating SHM segments requires a
  // round-trip to the browser process, we keep allocation out of the
  // steady-state of the decoder.
  std::vector<SHMBuffer*> available_shm_segments_;

  std::map<int32_t, PendingDecoderBuffer> bitstream_buffers_in_decoder_;
  PictureBufferMap assigned_picture_buffers_;
  // PictureBuffers given to us by VDA via PictureReady, which we sent forward
  // as VideoFrames to be rendered via decode_cb_, and which will be returned
  // to us via ReusePictureBuffer.
  typedef std::map<int32_t /* picture_buffer_id */,
                   PictureBuffer::TextureIds /* texture_id */>
      PictureBufferTextureMap;
  PictureBufferTextureMap picture_buffers_at_display_;

  // The texture target used for decoded pictures.
  uint32_t decoder_texture_target_;

  // The pixel format used for decoded pictures.
  VideoPixelFormat pixel_format_;

  struct BufferData {
    BufferData(int32_t bbid,
               base::TimeDelta ts,
               const gfx::Rect& visible_rect,
               const gfx::Size& natural_size);
    ~BufferData();
    int32_t bitstream_buffer_id;
    base::TimeDelta timestamp;
    gfx::Rect visible_rect;
    gfx::Size natural_size;
  };
  std::list<BufferData> input_buffer_data_;

  // picture_buffer_id and the frame wrapping the corresponding Picture, for
  // frames that have been decoded but haven't been requested by a Decode() yet.
  int32_t next_picture_buffer_id_;
  int32_t next_bitstream_buffer_id_;

  // Set during ProvidePictureBuffers(), used for checking and implementing
  // HasAvailableOutputFrames().
  int available_pictures_;

  // If true, the client cannot expect the VDA to produce any new decoded
  // frames, until it returns all PictureBuffers it may be holding back to the
  // VDA. In other words, the VDA may require all PictureBuffers to be able to
  // proceed with decoding the next frame.
  bool needs_all_picture_buffers_to_decode_;

  // If true, then the VDA supports deferred initialization via
  // NotifyInitializationComplete.  Otherwise, it will return initialization
  // status synchronously from VDA::Initialize.
  bool supports_deferred_initialization_;

  // This flag translates to COPY_REQUIRED flag for each frame.
  bool requires_texture_copy_;

  // Bound to factories_->GetMessageLoop().
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<GpuVideoDecoder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_GPU_VIDEO_DECODER_H_
