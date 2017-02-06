// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_IPC_CLIENT_GPU_VIDEO_ENCODE_ACCELERATOR_HOST_H_
#define MEDIA_GPU_IPC_CLIENT_GPU_VIDEO_ENCODE_ACCELERATOR_HOST_H_

#include <stdint.h>

#include <vector>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "ipc/ipc_listener.h"
#include "media/video/video_encode_accelerator.h"

namespace gfx {
struct GpuMemoryBufferHandle;
class Size;
}  // namespace gfx

namespace gpu {
class GpuChannelHost;
}  // namespace gpu

namespace media {
class VideoFrame;
}  // namespace media

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace media {

// This class is the renderer-side host for the VideoEncodeAccelerator in the
// GPU process, coordinated over IPC.
class GpuVideoEncodeAcceleratorHost
    : public IPC::Listener,
      public VideoEncodeAccelerator,
      public gpu::CommandBufferProxyImpl::DeletionObserver,
      public base::NonThreadSafe {
 public:
  // |this| is guaranteed not to outlive |impl|.  (See comments for |impl_|.)
  explicit GpuVideoEncodeAcceleratorHost(gpu::CommandBufferProxyImpl* impl);

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelError() override;

  // VideoEncodeAccelerator implementation.
  SupportedProfiles GetSupportedProfiles() override;
  bool Initialize(VideoPixelFormat input_format,
                  const gfx::Size& input_visible_size,
                  VideoCodecProfile output_profile,
                  uint32_t initial_bitrate,
                  Client* client) override;
  void Encode(const scoped_refptr<VideoFrame>& frame,
              bool force_keyframe) override;
  void UseOutputBitstreamBuffer(const BitstreamBuffer& buffer) override;
  void RequestEncodingParametersChange(uint32_t bitrate,
                                       uint32_t framerate_num) override;
  void Destroy() override;

  // gpu::CommandBufferProxyImpl::DeletionObserver implementation.
  void OnWillDeleteImpl() override;

 private:
  // Only Destroy() should be deleting |this|.
  ~GpuVideoEncodeAcceleratorHost() override;

  // Encode specific video frame types.
  void EncodeGpuMemoryBufferFrame(const scoped_refptr<VideoFrame>& frame,
                                  bool force_keyframe);
  void EncodeSharedMemoryFrame(const scoped_refptr<VideoFrame>& frame,
                               bool force_keyframe);

  // Notify |client_| of an error.  Posts a task to avoid re-entrancy.
  void PostNotifyError(const tracked_objects::Location& location,
                       Error error,
                       const std::string& message);

  void Send(IPC::Message* message);

  // IPC handlers, proxying VideoEncodeAccelerator::Client for the GPU
  // process.  Should not be called directly.
  void OnRequireBitstreamBuffers(uint32_t input_count,
                                 const gfx::Size& input_coded_size,
                                 uint32_t output_buffer_size);
  void OnNotifyInputDone(int32_t frame_id);
  void OnBitstreamBufferReady(int32_t bitstream_buffer_id,
                              uint32_t payload_size,
                              bool key_frame,
                              base::TimeDelta timestamp);
  void OnNotifyError(Error error);

  scoped_refptr<gpu::GpuChannelHost> channel_;

  // Route ID for the associated encoder in the GPU process.
  int32_t encoder_route_id_;

  // The client that will receive callbacks from the encoder.
  Client* client_;

  // Unowned reference to the gpu::CommandBufferProxyImpl that created us.
  // |this| registers as a DeletionObserver of |impl_|, so the reference is
  // always valid as long as it is not NULL.
  gpu::CommandBufferProxyImpl* impl_;

  // VideoFrames sent to the encoder.
  // base::IDMap not used here, since that takes pointers, not scoped_refptr.
  typedef base::hash_map<int32_t, scoped_refptr<VideoFrame>> FrameMap;
  FrameMap frame_map_;

  // ID serial number for the next frame to send to the GPU process.
  int32_t next_frame_id_;

  // WeakPtr factory for posting tasks back to itself.
  base::WeakPtrFactory<GpuVideoEncodeAcceleratorHost> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoEncodeAcceleratorHost);
};

}  // namespace media

#endif  // MEDIA_GPU_IPC_CLIENT_GPU_VIDEO_ENCODE_ACCELERATOR_HOST_H_
