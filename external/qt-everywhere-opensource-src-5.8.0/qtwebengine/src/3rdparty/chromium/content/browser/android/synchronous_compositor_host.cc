// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/synchronous_compositor_host.h"

#include <utility>

#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/output/compositor_frame_ack.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/android/sync_compositor_messages.h"
#include "content/common/android/sync_compositor_statics.h"
#include "content/public/browser/android/synchronous_compositor_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_sender.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/skia_util.h"

namespace content {

// static
void SynchronousCompositor::SetClientForWebContents(
    WebContents* contents,
    SynchronousCompositorClient* client) {
  DCHECK(contents);
  DCHECK(client);
  WebContentsAndroid* web_contents_android =
      static_cast<WebContentsImpl*>(contents)->GetWebContentsAndroid();
  DCHECK(!web_contents_android->synchronous_compositor_client());
  web_contents_android->set_synchronous_compositor_client(client);
}

// static
std::unique_ptr<SynchronousCompositorHost> SynchronousCompositorHost::Create(
    RenderWidgetHostViewAndroid* rwhva,
    WebContents* web_contents) {
  DCHECK(web_contents);
  WebContentsAndroid* web_contents_android =
      static_cast<WebContentsImpl*>(web_contents)->GetWebContentsAndroid();
  if (!web_contents_android->synchronous_compositor_client())
    return nullptr;  // Not using sync compositing.

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool use_in_proc_software_draw =
      command_line->HasSwitch(switches::kSingleProcess);
  return base::WrapUnique(new SynchronousCompositorHost(
      rwhva, web_contents_android->synchronous_compositor_client(),
      use_in_proc_software_draw));
}

SynchronousCompositorHost::SynchronousCompositorHost(
    RenderWidgetHostViewAndroid* rwhva,
    SynchronousCompositorClient* client,
    bool use_in_proc_software_draw)
    : rwhva_(rwhva),
      client_(client),
      ui_task_runner_(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI)),
      process_id_(rwhva_->GetRenderWidgetHost()->GetProcess()->GetID()),
      routing_id_(rwhva_->GetRenderWidgetHost()->GetRoutingID()),
      sender_(rwhva_->GetRenderWidgetHost()),
      use_in_process_zero_copy_software_draw_(use_in_proc_software_draw),
      bytes_limit_(0u),
      renderer_param_version_(0u),
      need_animate_scroll_(false),
      need_invalidate_count_(0u),
      did_activate_pending_tree_count_(0u) {
  client_->DidInitializeCompositor(this, process_id_, routing_id_);
}

SynchronousCompositorHost::~SynchronousCompositorHost() {
  client_->DidDestroyCompositor(this, process_id_, routing_id_);
}

bool SynchronousCompositorHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SynchronousCompositorHost, message)
    IPC_MESSAGE_HANDLER(SyncCompositorHostMsg_OutputSurfaceCreated,
                        OutputSurfaceCreated)
    IPC_MESSAGE_HANDLER(SyncCompositorHostMsg_UpdateState, ProcessCommonParams)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

SynchronousCompositor::Frame SynchronousCompositorHost::DemandDrawHw(
    const gfx::Size& surface_size,
    const gfx::Transform& transform,
    const gfx::Rect& viewport,
    const gfx::Rect& clip,
    const gfx::Rect& viewport_rect_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority) {
  SyncCompositorDemandDrawHwParams params(surface_size, transform, viewport,
                                          clip, viewport_rect_for_tile_priority,
                                          transform_for_tile_priority);
  SynchronousCompositor::Frame frame;
  frame.frame.reset(new cc::CompositorFrame);
  SyncCompositorCommonRendererParams common_renderer_params;
  if (!sender_->Send(new SyncCompositorMsg_DemandDrawHw(
          routing_id_, params, &common_renderer_params,
          &frame.output_surface_id, frame.frame.get()))) {
    return SynchronousCompositor::Frame();
  }
  ProcessCommonParams(common_renderer_params);
  if (!frame.frame->delegated_frame_data) {
    // This can happen if compositor did not swap in this draw.
    frame.frame.reset();
  }
  if (frame.frame) {
    UpdateFrameMetaData(frame.frame->metadata.Clone());
  }
  return frame;
}

void SynchronousCompositorHost::UpdateFrameMetaData(
    cc::CompositorFrameMetadata frame_metadata) {
  rwhva_->SynchronousFrameMetadata(std::move(frame_metadata));
}

namespace {

class ScopedSetSkCanvas {
 public:
  explicit ScopedSetSkCanvas(SkCanvas* canvas) {
    SynchronousCompositorSetSkCanvas(canvas);
  }

  ~ScopedSetSkCanvas() {
    SynchronousCompositorSetSkCanvas(nullptr);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedSetSkCanvas);
};

}

bool SynchronousCompositorHost::DemandDrawSwInProc(SkCanvas* canvas) {
  SyncCompositorCommonRendererParams common_renderer_params;
  bool success = false;
  std::unique_ptr<cc::CompositorFrame> frame(new cc::CompositorFrame);
  ScopedSetSkCanvas set_sk_canvas(canvas);
  SyncCompositorDemandDrawSwParams params;  // Unused.
  if (!sender_->Send(new SyncCompositorMsg_DemandDrawSw(
          routing_id_, params, &success, &common_renderer_params,
          frame.get()))) {
    return false;
  }
  if (!success)
    return false;
  ProcessCommonParams(common_renderer_params);
  UpdateFrameMetaData(std::move(frame->metadata));
  return true;
}

class SynchronousCompositorHost::ScopedSendZeroMemory {
 public:
  ScopedSendZeroMemory(SynchronousCompositorHost* host) : host_(host) {}
  ~ScopedSendZeroMemory() { host_->SendZeroMemory(); }

 private:
  SynchronousCompositorHost* const host_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSendZeroMemory);
};

struct SynchronousCompositorHost::SharedMemoryWithSize {
  base::SharedMemory shm;
  const size_t stride;
  const size_t buffer_size;

  SharedMemoryWithSize(size_t stride, size_t buffer_size)
      : stride(stride), buffer_size(buffer_size) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedMemoryWithSize);
};

bool SynchronousCompositorHost::DemandDrawSw(SkCanvas* canvas) {
  if (use_in_process_zero_copy_software_draw_)
    return DemandDrawSwInProc(canvas);

  SyncCompositorDemandDrawSwParams params;
  params.size = gfx::Size(canvas->getBaseLayerSize().width(),
                          canvas->getBaseLayerSize().height());
  SkIRect canvas_clip;
  canvas->getClipDeviceBounds(&canvas_clip);
  params.clip = gfx::SkIRectToRect(canvas_clip);
  params.transform.matrix() = canvas->getTotalMatrix();
  if (params.size.IsEmpty())
    return true;

  SkImageInfo info =
      SkImageInfo::MakeN32Premul(params.size.width(), params.size.height());
  DCHECK_EQ(kRGBA_8888_SkColorType, info.colorType());
  size_t stride = info.minRowBytes();
  size_t buffer_size = info.getSafeSize(stride);
  if (!buffer_size)
    return false;  // Overflow.

  SetSoftwareDrawSharedMemoryIfNeeded(stride, buffer_size);
  if (!software_draw_shm_)
    return false;

  std::unique_ptr<cc::CompositorFrame> frame(new cc::CompositorFrame);
  SyncCompositorCommonRendererParams common_renderer_params;
  bool success = false;
  if (!sender_->Send(new SyncCompositorMsg_DemandDrawSw(
          routing_id_, params, &success, &common_renderer_params,
          frame.get()))) {
    return false;
  }
  ScopedSendZeroMemory send_zero_memory(this);
  if (!success)
    return false;

  ProcessCommonParams(common_renderer_params);
  UpdateFrameMetaData(std::move(frame->metadata));

  SkBitmap bitmap;
  if (!bitmap.installPixels(info, software_draw_shm_->shm.memory(), stride))
    return false;

  {
    TRACE_EVENT0("browser", "DrawBitmap");
    canvas->save();
    canvas->resetMatrix();
    canvas->drawBitmap(bitmap, 0, 0);
    canvas->restore();
  }

  return true;
}

void SynchronousCompositorHost::SetSoftwareDrawSharedMemoryIfNeeded(
    size_t stride,
    size_t buffer_size) {
  if (software_draw_shm_ && software_draw_shm_->stride == stride &&
      software_draw_shm_->buffer_size == buffer_size)
    return;
  software_draw_shm_.reset();
  std::unique_ptr<SharedMemoryWithSize> software_draw_shm(
      new SharedMemoryWithSize(stride, buffer_size));
  {
    TRACE_EVENT1("browser", "AllocateSharedMemory", "buffer_size", buffer_size);
    if (!software_draw_shm->shm.CreateAndMapAnonymous(buffer_size))
      return;
  }

  SyncCompositorSetSharedMemoryParams set_shm_params;
  set_shm_params.buffer_size = buffer_size;
  base::ProcessHandle renderer_process_handle =
      rwhva_->GetRenderWidgetHost()->GetProcess()->GetHandle();
  if (!software_draw_shm->shm.ShareToProcess(renderer_process_handle,
                                             &set_shm_params.shm_handle)) {
    return;
  }

  bool success = false;
  SyncCompositorCommonRendererParams common_renderer_params;
  if (!sender_->Send(new SyncCompositorMsg_SetSharedMemory(
          routing_id_, set_shm_params, &success, &common_renderer_params)) ||
      !success) {
    return;
  }
  software_draw_shm_ = std::move(software_draw_shm);
  ProcessCommonParams(common_renderer_params);
}

void SynchronousCompositorHost::SendZeroMemory() {
  // No need to check return value.
  sender_->Send(new SyncCompositorMsg_ZeroSharedMemory(routing_id_));
}

void SynchronousCompositorHost::ReturnResources(
    uint32_t output_surface_id,
    const cc::CompositorFrameAck& frame_ack) {
  DCHECK(!frame_ack.resources.empty());
  sender_->Send(new SyncCompositorMsg_ReclaimResources(
      routing_id_, output_surface_id, frame_ack));
}

void SynchronousCompositorHost::SetMemoryPolicy(size_t bytes_limit) {
  if (bytes_limit_ == bytes_limit)
    return;

  if (sender_->Send(
          new SyncCompositorMsg_SetMemoryPolicy(routing_id_, bytes_limit))) {
    bytes_limit_ = bytes_limit;
  }
}

void SynchronousCompositorHost::DidChangeRootLayerScrollOffset(
    const gfx::ScrollOffset& root_offset) {
  if (root_scroll_offset_ == root_offset)
    return;
  root_scroll_offset_ = root_offset;
  sender_->Send(
      new SyncCompositorMsg_SetScroll(routing_id_, root_scroll_offset_));
}

void SynchronousCompositorHost::SynchronouslyZoomBy(float zoom_delta,
                                                    const gfx::Point& anchor) {
  SyncCompositorCommonRendererParams common_renderer_params;
  if (!sender_->Send(new SyncCompositorMsg_ZoomBy(
          routing_id_, zoom_delta, anchor, &common_renderer_params))) {
    return;
  }
  ProcessCommonParams(common_renderer_params);
}

void SynchronousCompositorHost::OnComputeScroll(
    base::TimeTicks animation_time) {
  if (!need_animate_scroll_)
    return;
  need_animate_scroll_ = false;

  SyncCompositorCommonRendererParams common_renderer_params;
  sender_->Send(
      new SyncCompositorMsg_ComputeScroll(routing_id_, animation_time));
}

void SynchronousCompositorHost::DidOverscroll(
    const DidOverscrollParams& over_scroll_params) {
  client_->DidOverscroll(this, over_scroll_params.accumulated_overscroll,
                         over_scroll_params.latest_overscroll_delta,
                         over_scroll_params.current_fling_velocity);
}

void SynchronousCompositorHost::DidSendBeginFrame() {
  SyncCompositorCommonRendererParams common_renderer_params;
  if (!sender_->Send(new SyncCompositorMsg_SynchronizeRendererState(
          routing_id_, &common_renderer_params))) {
    return;
  }
  ProcessCommonParams(common_renderer_params);
}

void SynchronousCompositorHost::OutputSurfaceCreated() {
  // New output surface is not aware of state from Browser side. So need to
  // re-send all browser side state here.
  sender_->Send(
      new SyncCompositorMsg_SetMemoryPolicy(routing_id_, bytes_limit_));
}

void SynchronousCompositorHost::ProcessCommonParams(
    const SyncCompositorCommonRendererParams& params) {
  // Ignore if |renderer_param_version_| is newer than |params.version|. This
  // comparison takes into account when the unsigned int wraps.
  if ((renderer_param_version_ - params.version) < 0x80000000) {
    return;
  }
  renderer_param_version_ = params.version;
  need_animate_scroll_ = params.need_animate_scroll;
  root_scroll_offset_ = params.total_scroll_offset;

  if (need_invalidate_count_ != params.need_invalidate_count) {
    need_invalidate_count_ = params.need_invalidate_count;
    client_->PostInvalidate(this);
  }

  if (did_activate_pending_tree_count_ !=
      params.did_activate_pending_tree_count) {
    did_activate_pending_tree_count_ = params.did_activate_pending_tree_count;
    client_->DidUpdateContent(this);
  }

  // Ensure only valid values from compositor are sent to client.
  // Compositor has page_scale_factor set to 0 before initialization, so check
  // for that case here.
  if (params.page_scale_factor) {
    client_->UpdateRootLayerState(
        this, gfx::ScrollOffsetToVector2dF(params.total_scroll_offset),
        gfx::ScrollOffsetToVector2dF(params.max_scroll_offset),
        params.scrollable_size, params.page_scale_factor,
        params.min_page_scale_factor, params.max_page_scale_factor);
  }
}

}  // namespace content
