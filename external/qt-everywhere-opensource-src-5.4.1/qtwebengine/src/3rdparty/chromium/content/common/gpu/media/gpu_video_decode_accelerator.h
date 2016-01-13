// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_GPU_VIDEO_DECODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_GPU_VIDEO_DECODE_ACCELERATOR_H_

#include <map>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/synchronization/waitable_event.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "media/video/video_decode_accelerator.h"
#include "ui/gfx/size.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

class GpuVideoDecodeAccelerator
    : public IPC::Listener,
      public IPC::Sender,
      public media::VideoDecodeAccelerator::Client,
      public GpuCommandBufferStub::DestructionObserver {
 public:
  // Each of the arguments to the constructor must outlive this object.
  // |stub->decoder()| will be made current around any operation that touches
  // the underlying VDA so that it can make GL calls safely.
  GpuVideoDecodeAccelerator(
      int32 host_route_id,
      GpuCommandBufferStub* stub,
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop);

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // media::VideoDecodeAccelerator::Client implementation.
  virtual void ProvidePictureBuffers(uint32 requested_num_of_buffers,
                                     const gfx::Size& dimensions,
                                     uint32 texture_target) OVERRIDE;
  virtual void DismissPictureBuffer(int32 picture_buffer_id) OVERRIDE;
  virtual void PictureReady(const media::Picture& picture) OVERRIDE;
  virtual void NotifyError(media::VideoDecodeAccelerator::Error error) OVERRIDE;
  virtual void NotifyEndOfBitstreamBuffer(int32 bitstream_buffer_id) OVERRIDE;
  virtual void NotifyFlushDone() OVERRIDE;
  virtual void NotifyResetDone() OVERRIDE;

  // GpuCommandBufferStub::DestructionObserver implementation.
  virtual void OnWillDestroyStub() OVERRIDE;

  // Function to delegate sending to actual sender.
  virtual bool Send(IPC::Message* message) OVERRIDE;

  // Initialize the accelerator with the given profile and send the
  // |init_done_msg| when done.
  void Initialize(const media::VideoCodecProfile profile,
                  IPC::Message* init_done_msg);

 private:
  class MessageFilter;

  // We only allow self-delete, from OnWillDestroyStub(), after cleanup there.
  virtual ~GpuVideoDecodeAccelerator();

  // Handlers for IPC messages.
  void OnDecode(base::SharedMemoryHandle handle, int32 id, uint32 size);
  void OnAssignPictureBuffers(const std::vector<int32>& buffer_ids,
                              const std::vector<uint32>& texture_ids);
  void OnReusePictureBuffer(int32 picture_buffer_id);
  void OnFlush();
  void OnReset();
  void OnDestroy();

  // Called on IO thread when |filter_| has been removed.
  void OnFilterRemoved();

  // Sets the texture to cleared.
  void SetTextureCleared(const media::Picture& picture);

  // Helper for replying to the creation request.
  void SendCreateDecoderReply(IPC::Message* message, bool succeeded);

  // Route ID to communicate with the host.
  int32 host_route_id_;

  // Unowned pointer to the underlying GpuCommandBufferStub.  |this| is
  // registered as a DestuctionObserver of |stub_| and will self-delete when
  // |stub_| is destroyed.
  GpuCommandBufferStub* stub_;

  // The underlying VideoDecodeAccelerator.
  scoped_ptr<media::VideoDecodeAccelerator> video_decode_accelerator_;

  // Callback for making the relevant context current for GL calls.
  // Returns false if failed.
  base::Callback<bool(void)> make_context_current_;

  // The texture dimensions as requested by ProvidePictureBuffers().
  gfx::Size texture_dimensions_;

  // The texture target as requested by ProvidePictureBuffers().
  uint32 texture_target_;

  // The message filter to run VDA::Decode on IO thread if VDA supports it.
  scoped_refptr<MessageFilter> filter_;

  // Used to wait on for |filter_| to be removed, before we can safely
  // destroy the VDA.
  base::WaitableEvent filter_removed_;

  // GPU child message loop.
  scoped_refptr<base::MessageLoopProxy> child_message_loop_;

  // GPU IO message loop.
  scoped_refptr<base::MessageLoopProxy> io_message_loop_;

  // Weak pointers will be invalidated on IO thread.
  base::WeakPtrFactory<Client> weak_factory_for_io_;

  // Protects |uncleared_textures_| when DCHECK is on. This is for debugging
  // only. We don't want to hold a lock on IO thread. When DCHECK is off,
  // |uncleared_textures_| is only accessed from the child thread.
  base::Lock debug_uncleared_textures_lock_;

  // A map from picture buffer ID to TextureRef that have not been cleared.
  std::map<int32, scoped_refptr<gpu::gles2::TextureRef> > uncleared_textures_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(GpuVideoDecodeAccelerator);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_GPU_VIDEO_DECODE_ACCELERATOR_H_
