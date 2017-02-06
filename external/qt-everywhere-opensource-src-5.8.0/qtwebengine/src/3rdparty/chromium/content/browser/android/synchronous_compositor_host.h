// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_HOST_H_
#define CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "cc/output/compositor_frame.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/geometry/size_f.h"

namespace IPC {
class Message;
class Sender;
}

namespace blink {
class WebInputEvent;
}

namespace cc {
struct BeginFrameArgs;
}

namespace content {

class RenderWidgetHostViewAndroid;
class SynchronousCompositorClient;
class WebContents;
struct DidOverscrollParams;
struct SyncCompositorCommonRendererParams;

class SynchronousCompositorHost : public SynchronousCompositor {
 public:
  static std::unique_ptr<SynchronousCompositorHost> Create(
      RenderWidgetHostViewAndroid* rwhva,
      WebContents* web_contents);

  ~SynchronousCompositorHost() override;

  // SynchronousCompositor overrides.
  SynchronousCompositor::Frame DemandDrawHw(
      const gfx::Size& surface_size,
      const gfx::Transform& transform,
      const gfx::Rect& viewport,
      const gfx::Rect& clip,
      const gfx::Rect& viewport_rect_for_tile_priority,
      const gfx::Transform& transform_for_tile_priority) override;
  bool DemandDrawSw(SkCanvas* canvas) override;
  void ReturnResources(uint32_t output_surface_id,
                       const cc::CompositorFrameAck& frame_ack) override;
  void SetMemoryPolicy(size_t bytes_limit) override;
  void DidChangeRootLayerScrollOffset(
      const gfx::ScrollOffset& root_offset) override;
  void SynchronouslyZoomBy(float zoom_delta, const gfx::Point& anchor) override;
  void OnComputeScroll(base::TimeTicks animation_time) override;

  void DidOverscroll(const DidOverscrollParams& over_scroll_params);
  void DidSendBeginFrame();
  bool OnMessageReceived(const IPC::Message& message);

 private:
  class ScopedSendZeroMemory;
  struct SharedMemoryWithSize;
  friend class ScopedSetZeroMemory;
  friend class SynchronousCompositorBase;

  SynchronousCompositorHost(RenderWidgetHostViewAndroid* rwhva,
                            SynchronousCompositorClient* client,
                            bool use_in_proc_software_draw);
  void ProcessCommonParams(const SyncCompositorCommonRendererParams& params);
  void UpdateFrameMetaData(cc::CompositorFrameMetadata frame_metadata);
  void OutputSurfaceCreated();
  bool DemandDrawSwInProc(SkCanvas* canvas);
  void SetSoftwareDrawSharedMemoryIfNeeded(size_t stride, size_t buffer_size);
  void SendZeroMemory();

  RenderWidgetHostViewAndroid* const rwhva_;
  SynchronousCompositorClient* const client_;
  const scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  const int process_id_;
  const int routing_id_;
  IPC::Sender* const sender_;
  const bool use_in_process_zero_copy_software_draw_;

  size_t bytes_limit_;
  std::unique_ptr<SharedMemoryWithSize> software_draw_shm_;

  // Updated by both renderer and browser.
  gfx::ScrollOffset root_scroll_offset_;

  // From renderer.
  uint32_t renderer_param_version_;
  bool need_animate_scroll_;
  uint32_t need_invalidate_count_;
  uint32_t did_activate_pending_tree_count_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_HOST_H_
