// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_IPC_SERVICE_GPU_VIDEO_ENCODE_ACCELERATOR_H_
#define MEDIA_GPU_IPC_SERVICE_GPU_VIDEO_ENCODE_ACCELERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/service/gpu_command_buffer_stub.h"
#include "ipc/ipc_listener.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/geometry/size.h"

struct AcceleratedVideoEncoderMsg_Encode_Params;
struct AcceleratedVideoEncoderMsg_Encode_Params2;

namespace base {
class SharedMemory;
}  // namespace base

namespace gpu {
struct GpuPreferences;
}  // namespace gpu

namespace media {

// This class encapsulates the GPU process view of a VideoEncodeAccelerator,
// wrapping the platform-specific VideoEncodeAccelerator instance.  It handles
// IPC coming in from the renderer and passes it to the underlying VEA.
class GpuVideoEncodeAccelerator
    : public IPC::Listener,
      public VideoEncodeAccelerator::Client,
      public gpu::GpuCommandBufferStub::DestructionObserver {
 public:
  GpuVideoEncodeAccelerator(int32_t host_route_id,
                            gpu::GpuCommandBufferStub* stub);
  ~GpuVideoEncodeAccelerator() override;

  // Initialize this accelerator with the given parameters and send
  // |init_done_msg| when complete.
  bool Initialize(VideoPixelFormat input_format,
                  const gfx::Size& input_visible_size,
                  VideoCodecProfile output_profile,
                  uint32_t initial_bitrate);

  // IPC::Listener implementation
  bool OnMessageReceived(const IPC::Message& message) override;

  // VideoEncodeAccelerator::Client implementation.
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override;
  void BitstreamBufferReady(int32_t bitstream_buffer_id,
                            size_t payload_size,
                            bool key_frame,
                            base::TimeDelta timestamp) override;
  void NotifyError(VideoEncodeAccelerator::Error error) override;

  // gpu::GpuCommandBufferStub::DestructionObserver implementation.
  void OnWillDestroyStub() override;

  // Static query for supported profiles.  This query calls the appropriate
  // platform-specific version. The returned supported profiles vector will
  // not contain duplicates.
  static gpu::VideoEncodeAcceleratorSupportedProfiles GetSupportedProfiles(
      const gpu::GpuPreferences& gpu_preferences);

 private:
  typedef std::unique_ptr<VideoEncodeAccelerator> (*CreateVEAFp)();

  // Return a set of VEA Create function pointers applicable to the current
  // platform.
  static std::vector<CreateVEAFp> CreateVEAFps(
      const gpu::GpuPreferences& gpu_preferences);
#if defined(OS_CHROMEOS) && defined(USE_V4L2_CODEC)
  static std::unique_ptr<VideoEncodeAccelerator> CreateV4L2VEA();
#endif
#if defined(OS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY)
  static std::unique_ptr<VideoEncodeAccelerator> CreateVaapiVEA();
#endif
#if defined(OS_ANDROID) && defined(ENABLE_WEBRTC)
  static std::unique_ptr<VideoEncodeAccelerator> CreateAndroidVEA();
#endif
#if defined(OS_MACOSX)
  static std::unique_ptr<VideoEncodeAccelerator> CreateVTVEA();
#endif

  // IPC handlers, proxying VideoEncodeAccelerator for the renderer
  // process.
  void OnEncode(const AcceleratedVideoEncoderMsg_Encode_Params& params);
  void OnEncode2(const AcceleratedVideoEncoderMsg_Encode_Params2& params);
  void OnUseOutputBitstreamBuffer(int32_t buffer_id,
                                  base::SharedMemoryHandle buffer_handle,
                                  uint32_t buffer_size);
  void OnRequestEncodingParametersChange(uint32_t bitrate, uint32_t framerate);

  void OnDestroy();

  void EncodeFrameFinished(int32_t frame_id,
                           std::unique_ptr<base::SharedMemory> shm);
  void Send(IPC::Message* message);

  // Route ID to communicate with the host.
  const uint32_t host_route_id_;

  // Unowned pointer to the underlying gpu::GpuCommandBufferStub.  |this| is
  // registered as a DestuctionObserver of |stub_| and will self-delete when
  // |stub_| is destroyed.
  gpu::GpuCommandBufferStub* const stub_;

  // Owned pointer to the underlying VideoEncodeAccelerator.
  std::unique_ptr<VideoEncodeAccelerator> encoder_;
  base::Callback<bool(void)> make_context_current_;

  // Video encoding parameters.
  VideoPixelFormat input_format_;
  gfx::Size input_visible_size_;
  gfx::Size input_coded_size_;
  size_t output_buffer_size_;

  // Weak pointer for VideoFrames that refer back to |this|.
  base::WeakPtrFactory<GpuVideoEncodeAccelerator> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoEncodeAccelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_IPC_SERVICE_GPU_VIDEO_ENCODE_ACCELERATOR_H_
