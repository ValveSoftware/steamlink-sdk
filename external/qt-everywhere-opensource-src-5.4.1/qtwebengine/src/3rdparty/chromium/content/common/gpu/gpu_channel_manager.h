// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_CHANNEL_MANAGER_H_
#define CONTENT_COMMON_GPU_GPU_CHANNEL_MANAGER_H_

#include <deque>
#include <string>
#include <vector>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "build/build_config.h"
#include "content/common/gpu/devtools_gpu_instrumentation.h"
#include "content/common/gpu/gpu_memory_manager.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_surface.h"

namespace base {
class WaitableEvent;
}

namespace gfx {
class GLFence;
class GLShareGroup;
struct GpuMemoryBufferHandle;
}

namespace gpu {
namespace gles2 {
class MailboxManager;
class ProgramCache;
class ShaderTranslatorCache;
}
}

namespace IPC {
struct ChannelHandle;
}

struct GPUCreateCommandBufferConfig;

namespace content {
class GpuChannel;
class GpuWatchdog;
class MessageRouter;
class SyncPointManager;

// A GpuChannelManager is a thread responsible for issuing rendering commands
// managing the lifetimes of GPU channels and forwarding IPC requests from the
// browser process to them based on the corresponding renderer ID.
class GpuChannelManager : public IPC::Listener,
                          public IPC::Sender {
 public:
  GpuChannelManager(MessageRouter* router,
                    GpuWatchdog* watchdog,
                    base::MessageLoopProxy* io_message_loop,
                    base::WaitableEvent* shutdown_event);
  virtual ~GpuChannelManager();

  // Remove the channel for a particular renderer.
  void RemoveChannel(int client_id);

  // Listener overrides.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // Sender overrides.
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  bool HandleMessagesScheduled();
  uint64 MessagesProcessed();

  void LoseAllContexts();

  base::WeakPtrFactory<GpuChannelManager> weak_factory_;

  int GenerateRouteID();
  void AddRoute(int32 routing_id, IPC::Listener* listener);
  void RemoveRoute(int32 routing_id);

  gpu::gles2::ProgramCache* program_cache();
  gpu::gles2::ShaderTranslatorCache* shader_translator_cache();

  gpu::gles2::MailboxManager* mailbox_manager() { return mailbox_manager_.get(); }

  GpuMemoryManager* gpu_memory_manager() { return &gpu_memory_manager_; }

  GpuEventsDispatcher* gpu_devtools_events_dispatcher() {
    return &gpu_devtools_events_dispatcher_;
  }

  GpuChannel* LookupChannel(int32 client_id);

  SyncPointManager* sync_point_manager() { return sync_point_manager_.get(); }
  typedef base::hash_map<uint32, gfx::GLFence*> SyncPointGLFences;
  SyncPointGLFences sync_point_gl_fences_;

  gfx::GLSurface* GetDefaultOffscreenSurface();

 private:
  struct ImageOperation {
    ImageOperation(int32 sync_point, base::Closure callback);
    ~ImageOperation();

    int32 sync_point;
    base::Closure callback;
  };
  typedef base::ScopedPtrHashMap<int, GpuChannel> GpuChannelMap;
  typedef std::deque<ImageOperation*> ImageOperationQueue;

  // Message handlers.
  void OnEstablishChannel(int client_id, bool share_context);
  void OnCloseChannel(const IPC::ChannelHandle& channel_handle);
  void OnVisibilityChanged(
      int32 render_view_id, int32 client_id, bool visible);
  void OnCreateViewCommandBuffer(
      const gfx::GLSurfaceHandle& window,
      int32 render_view_id,
      int32 client_id,
      const GPUCreateCommandBufferConfig& init_params,
      int32 route_id);
  void CreateImage(
      gfx::PluginWindowHandle window, int32 client_id, int32 image_id);
  void OnCreateImage(
      gfx::PluginWindowHandle window, int32 client_id, int32 image_id);
  void DeleteImage(int32 client_id, int32 image_id);
  void OnDeleteImage(int32 client_id, int32 image_id, int32 sync_point);
  void OnDeleteImageSyncPointRetired(ImageOperation*);
  void OnLoadedShader(std::string shader);
  void OnCreateGpuMemoryBuffer(const gfx::GpuMemoryBufferHandle& handle,
                               const gfx::Size& size,
                               unsigned internalformat,
                               unsigned usage);
  void OnDestroyGpuMemoryBuffer(const gfx::GpuMemoryBufferHandle& handle,
                                int32 sync_point);

  void OnLoseAllContexts();

  scoped_refptr<base::MessageLoopProxy> io_message_loop_;
  base::WaitableEvent* shutdown_event_;

  // Used to send and receive IPC messages from the browser process.
  MessageRouter* const router_;

  // These objects manage channels to individual renderer processes there is
  // one channel for each renderer process that has connected to this GPU
  // process.
  GpuChannelMap gpu_channels_;
  scoped_refptr<gfx::GLShareGroup> share_group_;
  scoped_refptr<gpu::gles2::MailboxManager> mailbox_manager_;
  GpuMemoryManager gpu_memory_manager_;
  GpuEventsDispatcher gpu_devtools_events_dispatcher_;
  GpuWatchdog* watchdog_;
  scoped_refptr<SyncPointManager> sync_point_manager_;
  scoped_ptr<gpu::gles2::ProgramCache> program_cache_;
  scoped_refptr<gpu::gles2::ShaderTranslatorCache> shader_translator_cache_;
  scoped_refptr<gfx::GLSurface> default_offscreen_surface_;
  ImageOperationQueue image_operations_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannelManager);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_CHANNEL_MANAGER_H_
