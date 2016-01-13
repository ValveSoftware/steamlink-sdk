// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_VIDEO_ENCODE_ACCELERATOR_HOST_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_VIDEO_ENCODE_ACCELERATOR_HOST_H_

#include <vector>

#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "content/common/gpu/client/command_buffer_proxy_impl.h"
#include "ipc/ipc_listener.h"
#include "media/video/video_encode_accelerator.h"

namespace gfx {

class Size;

}  // namespace gfx

namespace media {

class VideoFrame;

}  // namespace media

namespace content {

class GpuChannelHost;

// This class is the renderer-side host for the VideoEncodeAccelerator in the
// GPU process, coordinated over IPC.
class GpuVideoEncodeAcceleratorHost
    : public IPC::Listener,
      public media::VideoEncodeAccelerator,
      public CommandBufferProxyImpl::DeletionObserver,
      public base::NonThreadSafe {
 public:
  // |this| is guaranteed not to outlive |channel| and |impl|.  (See comments
  // for |channel_| and |impl_|.)
  GpuVideoEncodeAcceleratorHost(GpuChannelHost* channel,
                                CommandBufferProxyImpl* impl);

  // Static query for the supported profiles.  This query proxies to
  // GpuVideoEncodeAccelerator::GetSupportedProfiles().
  static std::vector<SupportedProfile> GetSupportedProfiles();

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // media::VideoEncodeAccelerator implementation.
  virtual bool Initialize(media::VideoFrame::Format input_format,
                          const gfx::Size& input_visible_size,
                          media::VideoCodecProfile output_profile,
                          uint32 initial_bitrate,
                          Client* client) OVERRIDE;
  virtual void Encode(const scoped_refptr<media::VideoFrame>& frame,
                      bool force_keyframe) OVERRIDE;
  virtual void UseOutputBitstreamBuffer(
      const media::BitstreamBuffer& buffer) OVERRIDE;
  virtual void RequestEncodingParametersChange(uint32 bitrate,
                                               uint32 framerate_num) OVERRIDE;
  virtual void Destroy() OVERRIDE;

  // CommandBufferProxyImpl::DeletionObserver implemetnation.
  virtual void OnWillDeleteImpl() OVERRIDE;

 private:
  // Only Destroy() should be deleting |this|.
  virtual ~GpuVideoEncodeAcceleratorHost();

  // Notify |client_| of an error.  Posts a task to avoid re-entrancy.
  void PostNotifyError(Error);

  void Send(IPC::Message* message);

  // IPC handlers, proxying media::VideoEncodeAccelerator::Client for the GPU
  // process.  Should not be called directly.
  void OnRequireBitstreamBuffers(uint32 input_count,
                                 const gfx::Size& input_coded_size,
                                 uint32 output_buffer_size);
  void OnNotifyInputDone(int32 frame_id);
  void OnBitstreamBufferReady(int32 bitstream_buffer_id,
                              uint32 payload_size,
                              bool key_frame);
  void OnNotifyError(Error error);

  // Unowned reference to the GpuChannelHost to send IPC messages to the GPU
  // process.  |channel_| outlives |impl_|, so the reference is always valid as
  // long as it is not NULL.
  GpuChannelHost* channel_;

  // Route ID for the associated encoder in the GPU process.
  int32 encoder_route_id_;

  // The client that will receive callbacks from the encoder.
  Client* client_;

  // Unowned reference to the CommandBufferProxyImpl that created us.  |this|
  // registers as a DeletionObserver of |impl_|, so the reference is always
  // valid as long as it is not NULL.
  CommandBufferProxyImpl* impl_;

  // media::VideoFrames sent to the encoder.
  // base::IDMap not used here, since that takes pointers, not scoped_refptr.
  typedef base::hash_map<int32, scoped_refptr<media::VideoFrame> > FrameMap;
  FrameMap frame_map_;

  // ID serial number for the next frame to send to the GPU process.
  int32 next_frame_id_;

  // WeakPtr factory for posting tasks back to itself.
  base::WeakPtrFactory<GpuVideoEncodeAcceleratorHost> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoEncodeAcceleratorHost);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_VIDEO_ENCODE_ACCELERATOR_HOST_H_
