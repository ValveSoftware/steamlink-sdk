// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_RENDERER_GPU_COMPOSITOR_OUTPUT_SURFACE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/output_surface.h"
#include "ipc/ipc_sync_message_filter.h"

namespace base {
class TaskRunner;
}

namespace IPC {
class ForwardingMessageFilter;
class Message;
}

namespace cc {
class CompositorFrame;
class CompositorFrameAck;
class GLFrameData;
class SoftwareFrameData;
}

namespace content {
class ContextProviderCommandBuffer;

// This class can be created only on the main thread, but then becomes pinned
// to a fixed thread when bindToClient is called.
class CompositorOutputSurface
    : NON_EXPORTED_BASE(public cc::OutputSurface),
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  static IPC::ForwardingMessageFilter* CreateFilter(
      base::TaskRunner* target_task_runner);

  CompositorOutputSurface(
      int32 routing_id,
      uint32 output_surface_id,
      const scoped_refptr<ContextProviderCommandBuffer>& context_provider,
      scoped_ptr<cc::SoftwareOutputDevice> software,
      bool use_swap_compositor_frame_message);
  virtual ~CompositorOutputSurface();

  // cc::OutputSurface implementation.
  virtual bool BindToClient(cc::OutputSurfaceClient* client) OVERRIDE;
  virtual void SwapBuffers(cc::CompositorFrame* frame) OVERRIDE;
#if defined(OS_ANDROID)
  virtual void SetNeedsBeginFrame(bool enable) OVERRIDE;
#endif

  // TODO(epenner): This seems out of place here and would be a better fit
  // int CompositorThread after it is fully refactored (http://crbug/170828)
  virtual void UpdateSmoothnessTakesPriority(bool prefer_smoothness) OVERRIDE;

 protected:
  void ShortcutSwapAck(uint32 output_surface_id,
                       scoped_ptr<cc::GLFrameData> gl_frame_data,
                       scoped_ptr<cc::SoftwareFrameData> software_frame_data);
  virtual void OnSwapAck(uint32 output_surface_id,
                         const cc::CompositorFrameAck& ack);
  virtual void OnReclaimResources(uint32 output_surface_id,
                                  const cc::CompositorFrameAck& ack);
  uint32 output_surface_id_;

 private:
  class CompositorOutputSurfaceProxy :
      public base::RefCountedThreadSafe<CompositorOutputSurfaceProxy> {
   public:
    explicit CompositorOutputSurfaceProxy(
        CompositorOutputSurface* output_surface)
        : output_surface_(output_surface) {}
    void ClearOutputSurface() { output_surface_ = NULL; }
    void OnMessageReceived(const IPC::Message& message) {
      if (output_surface_)
        output_surface_->OnMessageReceived(message);
    }

   private:
    friend class base::RefCountedThreadSafe<CompositorOutputSurfaceProxy>;
    ~CompositorOutputSurfaceProxy() {}
    CompositorOutputSurface* output_surface_;

    DISALLOW_COPY_AND_ASSIGN(CompositorOutputSurfaceProxy);
  };

  void OnMessageReceived(const IPC::Message& message);
  void OnUpdateVSyncParametersFromBrowser(base::TimeTicks timebase,
                                          base::TimeDelta interval);
#if defined(OS_ANDROID)
  void OnBeginFrame(const cc::BeginFrameArgs& args);
#endif
  bool Send(IPC::Message* message);

  bool use_swap_compositor_frame_message_;

  scoped_refptr<IPC::ForwardingMessageFilter> output_surface_filter_;
  scoped_refptr<CompositorOutputSurfaceProxy> output_surface_proxy_;
  scoped_refptr<IPC::SyncMessageFilter> message_sender_;
  int routing_id_;
  bool prefers_smoothness_;
  base::PlatformThreadHandle main_thread_handle_;

  // TODO(danakj): Remove this when crbug.com/311404
  bool layout_test_mode_;
  scoped_ptr<cc::CompositorFrameAck> layout_test_previous_frame_ack_;
  base::WeakPtrFactory<CompositorOutputSurface> weak_ptrs_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_COMPOSITOR_OUTPUT_SURFACE_H_
