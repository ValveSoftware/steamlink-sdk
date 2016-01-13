// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_PROCESS_HOST_UI_SHIM_H_
#define CONTENT_BROWSER_GPU_GPU_PROCESS_HOST_UI_SHIM_H_

// This class lives on the UI thread and supports classes like the
// BackingStoreProxy, which must live on the UI thread. The IO thread
// portion of this class, the GpuProcessHost, is responsible for
// shuttling messages between the browser and GPU processes.

#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "content/common/content_export.h"
#include "content/common/message_router.h"
#include "content/public/common/gpu_memory_stats.h"
#include "gpu/config/gpu_info.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"

struct GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params;
struct GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params;
struct GpuHostMsg_AcceleratedSurfaceRelease_Params;

namespace ui {
struct LatencyInfo;
}

namespace gfx {
class Size;
}

namespace IPC {
class Message;
}

namespace content {
void RouteToGpuProcessHostUIShimTask(int host_id, const IPC::Message& msg);

class GpuProcessHostUIShim : public IPC::Listener,
                             public IPC::Sender,
                             public base::NonThreadSafe {
 public:
  // Create a GpuProcessHostUIShim with the given ID.  The object can be found
  // using FromID with the same id.
  static GpuProcessHostUIShim* Create(int host_id);

  // Destroy the GpuProcessHostUIShim with the given host ID. This can only
  // be called on the UI thread. Only the GpuProcessHost should destroy the
  // UI shim.
  static void Destroy(int host_id, const std::string& message);

  // Destroy all remaining GpuProcessHostUIShims.
  CONTENT_EXPORT static void DestroyAll();

  CONTENT_EXPORT static GpuProcessHostUIShim* FromID(int host_id);

  // Get a GpuProcessHostUIShim instance; it doesn't matter which one.
  // Return NULL if none has been created.
  CONTENT_EXPORT static GpuProcessHostUIShim* GetOneInstance();

  // IPC::Sender implementation.
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // IPC::Listener implementation.
  // The GpuProcessHost causes this to be called on the UI thread to
  // dispatch the incoming messages from the GPU process, which are
  // actually received on the IO thread.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  CONTENT_EXPORT void SimulateRemoveAllContext();
  CONTENT_EXPORT void SimulateCrash();
  CONTENT_EXPORT void SimulateHang();

 private:
  explicit GpuProcessHostUIShim(int host_id);
  virtual ~GpuProcessHostUIShim();

  // Message handlers.
  bool OnControlMessageReceived(const IPC::Message& message);

  void OnLogMessage(int level, const std::string& header,
      const std::string& message);

  void OnGraphicsInfoCollected(const gpu::GPUInfo& gpu_info);

  void OnAcceleratedSurfaceInitialized(int32 surface_id, int32 route_id);
  void OnAcceleratedSurfaceBuffersSwapped(
      const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params);
  void OnAcceleratedSurfacePostSubBuffer(
      const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params);
  void OnAcceleratedSurfaceSuspend(int32 surface_id);
  void OnAcceleratedSurfaceRelease(
      const GpuHostMsg_AcceleratedSurfaceRelease_Params& params);
  void OnVideoMemoryUsageStatsReceived(
      const GPUVideoMemoryUsageStats& video_memory_usage_stats);
  void OnUpdateVSyncParameters(int surface_id,
                               base::TimeTicks timebase,
                               base::TimeDelta interval);
  void OnFrameDrawn(const std::vector<ui::LatencyInfo>& latency_info);

  // The serial number of the GpuProcessHost / GpuProcessHostUIShim pair.
  int host_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_PROCESS_HOST_UI_SHIM_H_
