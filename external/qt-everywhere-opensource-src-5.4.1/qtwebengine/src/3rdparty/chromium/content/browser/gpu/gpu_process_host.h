// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_PROCESS_HOST_H_
#define CONTENT_BROWSER_GPU_GPU_PROCESS_HOST_H_

#include <map>
#include <queue>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/common/content_export.h"
#include "content/common/gpu/gpu_memory_uma_stats.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/common/gpu/gpu_result_codes.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/browser/gpu_data_manager.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/config/gpu_info.h"
#include "ipc/ipc_sender.h"
#include "ipc/message_filter.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"
#include "url/gurl.h"

struct GPUCreateCommandBufferConfig;
struct GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params;
struct GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params;
struct GpuHostMsg_AcceleratedSurfaceRelease_Params;

namespace gfx {
struct GpuMemoryBufferHandle;
}

namespace IPC {
struct ChannelHandle;
}

namespace content {
class BrowserChildProcessHostImpl;
class GpuMainThread;
class RenderWidgetHostViewFrameSubscriber;
class ShaderDiskCache;

typedef base::Thread* (*GpuMainThreadFactoryFunction)(const std::string& id);

class GpuProcessHost : public BrowserChildProcessHostDelegate,
                       public IPC::Sender,
                       public base::NonThreadSafe {
 public:
  enum GpuProcessKind {
    GPU_PROCESS_KIND_UNSANDBOXED,
    GPU_PROCESS_KIND_SANDBOXED,
    GPU_PROCESS_KIND_COUNT
  };

  typedef base::Callback<void(const IPC::ChannelHandle&, const gpu::GPUInfo&)>
      EstablishChannelCallback;

  typedef base::Callback<void(CreateCommandBufferResult)>
      CreateCommandBufferCallback;

  typedef base::Callback<void(const gfx::Size)> CreateImageCallback;

  typedef base::Callback<void(const gfx::GpuMemoryBufferHandle& handle)>
      CreateGpuMemoryBufferCallback;

  static bool gpu_enabled() { return gpu_enabled_; }

  // Creates a new GpuProcessHost or gets an existing one, resulting in the
  // launching of a GPU process if required.  Returns null on failure. It
  // is not safe to store the pointer once control has returned to the message
  // loop as it can be destroyed. Instead store the associated GPU host ID.
  // This could return NULL if GPU access is not allowed (blacklisted).
  CONTENT_EXPORT static GpuProcessHost* Get(GpuProcessKind kind,
                                            CauseForGpuLaunch cause);

  // Retrieves a list of process handles for all gpu processes.
  static void GetProcessHandles(
      const GpuDataManager::GetGpuProcessHandlesCallback& callback);

  // Helper function to send the given message to the GPU process on the IO
  // thread.  Calls Get and if a host is returned, sends it.  Can be called from
  // any thread.  Deletes the message if it cannot be sent.
  CONTENT_EXPORT static void SendOnIO(GpuProcessKind kind,
                                      CauseForGpuLaunch cause,
                                      IPC::Message* message);

  CONTENT_EXPORT static void RegisterGpuMainThreadFactory(
      GpuMainThreadFactoryFunction create);

  // Get the GPU process host for the GPU process with the given ID. Returns
  // null if the process no longer exists.
  static GpuProcessHost* FromID(int host_id);
  int host_id() const { return host_id_; }

  // IPC::Sender implementation.
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // Adds a message filter to the GpuProcessHost's channel.
  void AddFilter(IPC::MessageFilter* filter);

  // Tells the GPU process to create a new channel for communication with a
  // client. Once the GPU process responds asynchronously with the IPC handle
  // and GPUInfo, we call the callback.
  void EstablishGpuChannel(int client_id,
                           bool share_context,
                           const EstablishChannelCallback& callback);

  // Tells the GPU process to create a new command buffer that draws into the
  // given surface.
  void CreateViewCommandBuffer(
      const gfx::GLSurfaceHandle& compositing_surface,
      int surface_id,
      int client_id,
      const GPUCreateCommandBufferConfig& init_params,
      int route_id,
      const CreateCommandBufferCallback& callback);

  // Tells the GPU process to create a new image using the given window.
  void CreateImage(
      gfx::PluginWindowHandle window,
      int client_id,
      int image_id,
      const CreateImageCallback& callback);

    // Tells the GPU process to delete image.
  void DeleteImage(int client_id, int image_id, int sync_point);

  void CreateGpuMemoryBuffer(const gfx::GpuMemoryBufferHandle& handle,
                             const gfx::Size& size,
                             unsigned internalformat,
                             unsigned usage,
                             const CreateGpuMemoryBufferCallback& callback);
  void DestroyGpuMemoryBuffer(const gfx::GpuMemoryBufferHandle& handle,
                              int sync_point);

  // What kind of GPU process, e.g. sandboxed or unsandboxed.
  GpuProcessKind kind();

  void ForceShutdown();

  void BeginFrameSubscription(
      int surface_id,
      base::WeakPtr<RenderWidgetHostViewFrameSubscriber> subscriber);
  void EndFrameSubscription(int surface_id);
  void LoadedShader(const std::string& key, const std::string& data);

 private:
  static bool ValidateHost(GpuProcessHost* host);

  GpuProcessHost(int host_id, GpuProcessKind kind);
  virtual ~GpuProcessHost();

  bool Init();

  // Post an IPC message to the UI shim's message handler on the UI thread.
  void RouteOnUIThread(const IPC::Message& message);

  // BrowserChildProcessHostDelegate implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnProcessLaunched() OVERRIDE;
  virtual void OnProcessCrashed(int exit_code) OVERRIDE;

  // Message handlers.
  void OnInitialized(bool result, const gpu::GPUInfo& gpu_info);
  void OnChannelEstablished(const IPC::ChannelHandle& channel_handle);
  void OnCommandBufferCreated(CreateCommandBufferResult result);
  void OnDestroyCommandBuffer(int32 surface_id);
  void OnImageCreated(const gfx::Size size);
  void OnGpuMemoryBufferCreated(const gfx::GpuMemoryBufferHandle& handle);
  void OnDidCreateOffscreenContext(const GURL& url);
  void OnDidLoseContext(bool offscreen,
                        gpu::error::ContextLostReason reason,
                        const GURL& url);
  void OnDidDestroyOffscreenContext(const GURL& url);
  void OnGpuMemoryUmaStatsReceived(const GPUMemoryUmaStats& stats);
#if defined(OS_MACOSX)
  void OnAcceleratedSurfaceBuffersSwapped(
      const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params);
#endif

  void CreateChannelCache(int32 client_id);
  void OnDestroyChannel(int32 client_id);
  void OnCacheShader(int32 client_id, const std::string& key,
                     const std::string& shader);

  bool LaunchGpuProcess(const std::string& channel_id);

  void SendOutstandingReplies();

  void BlockLiveOffscreenContexts();

  std::string GetShaderPrefixKey();

  // The serial number of the GpuProcessHost / GpuProcessHostUIShim pair.
  int host_id_;

  // These are the channel requests that we have already sent to
  // the GPU process, but haven't heard back about yet.
  std::queue<EstablishChannelCallback> channel_requests_;

  // The pending create command buffer requests we need to reply to.
  std::queue<CreateCommandBufferCallback> create_command_buffer_requests_;

  // The pending create image requests we need to reply to.
  std::queue<CreateImageCallback> create_image_requests_;

  // The pending create gpu memory buffer requests we need to reply to.
  std::queue<CreateGpuMemoryBufferCallback> create_gpu_memory_buffer_requests_;

  // Qeueud messages to send when the process launches.
  std::queue<IPC::Message*> queued_messages_;

  // Whether the GPU process is valid, set to false after Send() failed.
  bool valid_;

  // Whether we are running a GPU thread inside the browser process instead
  // of a separate GPU process.
  bool in_process_;

  bool swiftshader_rendering_;
  GpuProcessKind kind_;

  scoped_ptr<base::Thread> in_process_gpu_thread_;

  // Whether we actually launched a GPU process.
  bool process_launched_;

  // Whether the GPU process successfully initialized.
  bool initialized_;

  // Time Init started.  Used to log total GPU process startup time to UMA.
  base::TimeTicks init_start_time_;

  // Master switch for enabling/disabling GPU acceleration for the current
  // browser session. It does not change the acceleration settings for
  // existing tabs, just the future ones.
  static bool gpu_enabled_;

  static bool hardware_gpu_enabled_;

  scoped_ptr<BrowserChildProcessHostImpl> process_;

  // Track the URLs of the pages which have live offscreen contexts,
  // assumed to be associated with untrusted content such as WebGL.
  // For best robustness, when any context lost notification is
  // received, assume all of these URLs are guilty, and block
  // automatic execution of 3D content from those domains.
  std::multiset<GURL> urls_with_live_offscreen_contexts_;

  // Statics kept around to send to UMA histograms on GPU process lost.
  bool uma_memory_stats_received_;
  GPUMemoryUmaStats uma_memory_stats_;

  // This map of frame subscribers are listening for frame presentation events.
  // The key is the surface id and value is the subscriber.
  typedef base::hash_map<int,
                         base::WeakPtr<RenderWidgetHostViewFrameSubscriber> >
  FrameSubscriberMap;
  FrameSubscriberMap frame_subscribers_;

  typedef std::map<int32, scoped_refptr<ShaderDiskCache> >
      ClientIdToShaderCacheMap;
  ClientIdToShaderCacheMap client_id_to_shader_cache_;

  std::string shader_prefix_key_;

  // Keep an extra reference to the SurfaceRef stored in the GpuSurfaceTracker
  // in this map so that we don't destroy it whilst the GPU process is
  // drawing to it.
  typedef std::multimap<int, scoped_refptr<GpuSurfaceTracker::SurfaceRef> >
      SurfaceRefMap;
  SurfaceRefMap surface_refs_;

  DISALLOW_COPY_AND_ASSIGN(GpuProcessHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_PROCESS_HOST_H_
