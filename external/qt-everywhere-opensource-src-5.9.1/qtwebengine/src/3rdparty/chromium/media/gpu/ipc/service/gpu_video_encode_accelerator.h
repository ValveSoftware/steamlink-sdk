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
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/service/gpu_command_buffer_stub.h"
#include "ipc/ipc_listener.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/geometry/size.h"

struct AcceleratedVideoEncoderMsg_Encode_Params;

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
      public IPC::Sender,
      public VideoEncodeAccelerator::Client,
      public gpu::GpuCommandBufferStub::DestructionObserver {
 public:
  GpuVideoEncodeAccelerator(
      int32_t host_route_id,
      gpu::GpuCommandBufferStub* stub,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);
  ~GpuVideoEncodeAccelerator() override;

  // Initialize this accelerator with the given parameters and send
  // |init_done_msg| when complete.
  bool Initialize(VideoPixelFormat input_format,
                  const gfx::Size& input_visible_size,
                  VideoCodecProfile output_profile,
                  uint32_t initial_bitrate);

  // IPC::Listener implementation
  bool OnMessageReceived(const IPC::Message& message) override;

  // IPC::Sender implementation
  bool Send(IPC::Message* message) override;

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
  // Returns a vector of VEAFactoryFunctions for the current platform.
  using VEAFactoryFunction =
      base::Callback<std::unique_ptr<VideoEncodeAccelerator>()>;
  static std::vector<VEAFactoryFunction> GetVEAFactoryFunctions(
      const gpu::GpuPreferences& gpu_preferences);

  class MessageFilter;

  // Called on IO thread when |filter_| has been removed.
  void OnFilterRemoved();

  // IPC handlers, proxying VideoEncodeAccelerator for the renderer
  // process.
  void OnEncode(const AcceleratedVideoEncoderMsg_Encode_Params& params);
  void OnUseOutputBitstreamBuffer(int32_t buffer_id,
                                  base::SharedMemoryHandle buffer_handle,
                                  uint32_t buffer_size);
  void OnRequestEncodingParametersChange(uint32_t bitrate, uint32_t framerate);

  void OnDestroy();

  // Operations that run on encoder worker thread.
  void CreateEncodeFrameOnEncoderWorker(
      const AcceleratedVideoEncoderMsg_Encode_Params& params);
  void DestroyOnEncoderWorker();

  // Completes encode tasks with the received |frame|.
  void OnEncodeFrameCreated(int32_t frame_id,
                            bool force_keyframe,
                            const scoped_refptr<media::VideoFrame>& frame);

  // Notifies renderer that |frame_id| can be reused as input for encode is
  // completed.
  void EncodeFrameFinished(int32_t frame_id);

  // Checks that function is called on the correct thread. If MessageFilter is
  // used, checks if it is called on |io_task_runner_|. If not, checks if it is
  // called on |main_task_runner_|.
  bool CheckIfCalledOnCorrectThread();

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

  // The message filter to run VEA encode methods on IO thread if VEA supports
  // it.
  scoped_refptr<MessageFilter> filter_;

  // Used to wait on for |filter_| to be removed, before we can safely
  // destroy the VEA.
  base::WaitableEvent filter_removed_;

  // This thread services the operations necessary for encode so that they
  // wouldn't block |main_task_runner_| or |io_task_runner_|.
  base::Thread encoder_worker_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> encoder_worker_task_runner_;

  // GPU main thread task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // GPU IO thread task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Task runner used for posting encode tasks. If
  // TryToSetupEncodeOnSeperateThread() is true, |io_task_runner_| is used,
  // otherwise |main_thread_task_runner_|.
  scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner_;

  // Weak pointer for referring back to |this| on |encoder_worker_task_runner_|.
  base::WeakPtrFactory<GpuVideoEncodeAccelerator>
      weak_this_factory_for_encoder_worker_;

  // Weak pointer for VideoFrames that refer back to |this| on
  // |main_task_runner| or |io_task_runner_|.
  base::WeakPtrFactory<GpuVideoEncodeAccelerator> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoEncodeAccelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_IPC_SERVICE_GPU_VIDEO_ENCODE_ACCELERATOR_H_
