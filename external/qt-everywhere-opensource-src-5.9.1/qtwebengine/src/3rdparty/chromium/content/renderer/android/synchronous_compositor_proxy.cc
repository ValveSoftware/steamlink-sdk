// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/synchronous_compositor_proxy.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/memory/shared_memory.h"
#include "cc/ipc/cc_param_traits.h"
#include "content/common/android/sync_compositor_messages.h"
#include "content/common/android/sync_compositor_statics.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/skia_util.h"

namespace content {

SynchronousCompositorProxy::SynchronousCompositorProxy(
    int routing_id,
    IPC::Sender* sender,
    ui::SynchronousInputHandlerProxy* input_handler_proxy)
    : routing_id_(routing_id),
      sender_(sender),
      input_handler_proxy_(input_handler_proxy),
      use_in_process_zero_copy_software_draw_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kSingleProcess)),
      compositor_frame_sink_(nullptr),
      inside_receive_(false),
      hardware_draw_reply_(nullptr),
      software_draw_reply_(nullptr),
      hardware_draw_reply_async_(false),
      version_(0u),
      page_scale_factor_(0.f),
      min_page_scale_factor_(0.f),
      max_page_scale_factor_(0.f),
      need_animate_scroll_(false),
      need_invalidate_count_(0u),
      did_activate_pending_tree_count_(0u) {
  DCHECK(input_handler_proxy_);
  input_handler_proxy_->SetOnlySynchronouslyAnimateRootFlings(this);
}

SynchronousCompositorProxy::~SynchronousCompositorProxy() {
  // The CompositorFrameSink is destroyed/removed by the compositor before
  // shutting down everything.
  DCHECK_EQ(compositor_frame_sink_, nullptr);
  input_handler_proxy_->SetOnlySynchronouslyAnimateRootFlings(nullptr);
}

void SynchronousCompositorProxy::SetCompositorFrameSink(
    SynchronousCompositorFrameSink* compositor_frame_sink) {
  DCHECK_NE(compositor_frame_sink_, compositor_frame_sink);
  if (compositor_frame_sink_) {
    compositor_frame_sink_->SetSyncClient(nullptr);
  }
  compositor_frame_sink_ = compositor_frame_sink;
  if (compositor_frame_sink_) {
    compositor_frame_sink_->SetSyncClient(this);
  }
}

void SynchronousCompositorProxy::SetNeedsSynchronousAnimateInput() {
  need_animate_scroll_ = true;
  Invalidate();
}

void SynchronousCompositorProxy::UpdateRootLayerState(
    const gfx::ScrollOffset& total_scroll_offset,
    const gfx::ScrollOffset& max_scroll_offset,
    const gfx::SizeF& scrollable_size,
    float page_scale_factor,
    float min_page_scale_factor,
    float max_page_scale_factor) {
  if (total_scroll_offset_ != total_scroll_offset ||
      max_scroll_offset_ != max_scroll_offset ||
      scrollable_size_ != scrollable_size ||
      page_scale_factor_ != page_scale_factor ||
      min_page_scale_factor_ != min_page_scale_factor ||
      max_page_scale_factor_ != max_page_scale_factor) {
    total_scroll_offset_ = total_scroll_offset;
    max_scroll_offset_ = max_scroll_offset;
    scrollable_size_ = scrollable_size;
    page_scale_factor_ = page_scale_factor;
    min_page_scale_factor_ = min_page_scale_factor;
    max_page_scale_factor_ = max_page_scale_factor;

    SendAsyncRendererStateIfNeeded();
  }
}

void SynchronousCompositorProxy::Invalidate() {
  ++need_invalidate_count_;
  SendAsyncRendererStateIfNeeded();
}

void SynchronousCompositorProxy::DidActivatePendingTree() {
  ++did_activate_pending_tree_count_;
  SendAsyncRendererStateIfNeeded();
}

void SynchronousCompositorProxy::SendAsyncRendererStateIfNeeded() {
  if (inside_receive_)
    return;
  SyncCompositorCommonRendererParams params;
  PopulateCommonParams(&params);
  Send(new SyncCompositorHostMsg_UpdateState(routing_id_, params));
}

void SynchronousCompositorProxy::PopulateCommonParams(
    SyncCompositorCommonRendererParams* params) const {
  params->version = ++version_;
  params->total_scroll_offset = total_scroll_offset_;
  params->max_scroll_offset = max_scroll_offset_;
  params->scrollable_size = scrollable_size_;
  params->page_scale_factor = page_scale_factor_;
  params->min_page_scale_factor = min_page_scale_factor_;
  params->max_page_scale_factor = max_page_scale_factor_;
  params->need_animate_scroll = need_animate_scroll_;
  params->need_invalidate_count = need_invalidate_count_;
  params->did_activate_pending_tree_count = did_activate_pending_tree_count_;
}

void SynchronousCompositorProxy::OnMessageReceived(
    const IPC::Message& message) {
  if (compositor_frame_sink_ &&
      compositor_frame_sink_->OnMessageReceived(message))
    return;

  IPC_BEGIN_MESSAGE_MAP(SynchronousCompositorProxy, message)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_ComputeScroll, OnComputeScroll)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(SyncCompositorMsg_DemandDrawHw,
                                    DemandDrawHw)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_DemandDrawHwAsync, DemandDrawHwAsync)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_SetSharedMemory, SetSharedMemory)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_ZeroSharedMemory, ZeroSharedMemory)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(SyncCompositorMsg_DemandDrawSw,
                                    DemandDrawSw)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_ZoomBy, SynchronouslyZoomBy)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_SetScroll, SetScroll)
  IPC_END_MESSAGE_MAP()
}

bool SynchronousCompositorProxy::Send(IPC::Message* message) {
  return sender_->Send(message);
}

void SynchronousCompositorProxy::DemandDrawHwAsync(
    const SyncCompositorDemandDrawHwParams& params) {
  DoDemandDrawHw(params, nullptr);
}

void SynchronousCompositorProxy::DemandDrawHw(
    const SyncCompositorDemandDrawHwParams& params,
    IPC::Message* reply_message) {
  DCHECK(reply_message);
  DoDemandDrawHw(params, reply_message);
}

void SynchronousCompositorProxy::DoDemandDrawHw(
    const SyncCompositorDemandDrawHwParams& params,
    IPC::Message* reply_message) {
  DCHECK(!inside_receive_);
  inside_receive_ = true;

  if (compositor_frame_sink_) {
    if (!reply_message) {
      base::AutoReset<bool> scoped_hardware_draw_reply_async(
          &hardware_draw_reply_async_, true);
      compositor_frame_sink_->DemandDrawHw(
          params.viewport_size, params.viewport_rect_for_tile_priority,
          params.transform_for_tile_priority);
    } else {
      base::AutoReset<IPC::Message*> scoped_hardware_draw_reply(
          &hardware_draw_reply_, reply_message);
      compositor_frame_sink_->DemandDrawHw(
          params.viewport_size, params.viewport_rect_for_tile_priority,
          params.transform_for_tile_priority);
    }
  }

  if (inside_receive_) {
    // Did not swap.
    if (!reply_message) {
      SendDemandDrawHwReplyAsync(base::nullopt, 0u);
    } else {
      SendDemandDrawHwReply(base::nullopt, 0u, reply_message);
    }
    inside_receive_ = false;
  }
}

void SynchronousCompositorProxy::SubmitCompositorFrameHwAsync(
    uint32_t compositor_frame_sink_id,
    cc::CompositorFrame frame) {
  DCHECK(inside_receive_);
  DCHECK(hardware_draw_reply_async_);
  SendDemandDrawHwReplyAsync(std::move(frame), compositor_frame_sink_id);
  inside_receive_ = false;
}

void SynchronousCompositorProxy::SubmitCompositorFrameHw(
    uint32_t compositor_frame_sink_id,
    cc::CompositorFrame frame) {
  DCHECK(inside_receive_);
  DCHECK(hardware_draw_reply_);
  SendDemandDrawHwReply(std::move(frame), compositor_frame_sink_id,
                        hardware_draw_reply_);
  inside_receive_ = false;
}

void SynchronousCompositorProxy::SendDemandDrawHwReplyAsync(
    base::Optional<cc::CompositorFrame> frame,
    uint32_t compositor_frame_sink_id) {
  Send(new SyncCompositorHostMsg_ReturnFrame(routing_id_,
                                             compositor_frame_sink_id, frame));
}

void SynchronousCompositorProxy::SendDemandDrawHwReply(
    base::Optional<cc::CompositorFrame> frame,
    uint32_t compositor_frame_sink_id,
    IPC::Message* reply_message) {
  SyncCompositorCommonRendererParams common_renderer_params;
  PopulateCommonParams(&common_renderer_params);
  SyncCompositorMsg_DemandDrawHw::WriteReplyParams(
      reply_message, common_renderer_params, compositor_frame_sink_id, frame);
  Send(reply_message);
}

struct SynchronousCompositorProxy::SharedMemoryWithSize {
  base::SharedMemory shm;
  const size_t buffer_size;
  bool zeroed;

  SharedMemoryWithSize(base::SharedMemoryHandle shm_handle, size_t buffer_size)
      : shm(shm_handle, false), buffer_size(buffer_size), zeroed(true) {}
};

void SynchronousCompositorProxy::SetSharedMemory(
    const SyncCompositorSetSharedMemoryParams& params,
    bool* success,
    SyncCompositorCommonRendererParams* common_renderer_params) {
  DCHECK(!inside_receive_);
  base::AutoReset<bool> scoped_inside_receive(&inside_receive_, true);

  *success = false;
  if (!base::SharedMemory::IsHandleValid(params.shm_handle))
    return;

  software_draw_shm_.reset(
      new SharedMemoryWithSize(params.shm_handle, params.buffer_size));
  if (!software_draw_shm_->shm.Map(params.buffer_size))
    return;
  DCHECK(software_draw_shm_->shm.memory());
  PopulateCommonParams(common_renderer_params);
  *success = true;
}

void SynchronousCompositorProxy::ZeroSharedMemory() {
  // It is possible for this to get called twice, eg. if draw is called before
  // the CompositorFrameSink is ready. Just ignore duplicated calls rather than
  // inventing a complicated system to avoid it.
  if (software_draw_shm_->zeroed)
    return;

  memset(software_draw_shm_->shm.memory(), 0, software_draw_shm_->buffer_size);
  software_draw_shm_->zeroed = true;
}

void SynchronousCompositorProxy::DemandDrawSw(
    const SyncCompositorDemandDrawSwParams& params,
    IPC::Message* reply_message) {
  DCHECK(!inside_receive_);
  inside_receive_ = true;
  if (compositor_frame_sink_) {
    base::AutoReset<IPC::Message*> scoped_software_draw_reply(
        &software_draw_reply_, reply_message);
    SkCanvas* sk_canvas_for_draw = SynchronousCompositorGetSkCanvas();
    if (use_in_process_zero_copy_software_draw_) {
      DCHECK(sk_canvas_for_draw);
      compositor_frame_sink_->DemandDrawSw(sk_canvas_for_draw);
    } else {
      DCHECK(!sk_canvas_for_draw);
      DoDemandDrawSw(params);
    }
  }
  if (inside_receive_) {
    // Did not swap.
    SendDemandDrawSwReply(false, cc::CompositorFrame(), reply_message);
    inside_receive_ = false;
  }
}

void SynchronousCompositorProxy::DoDemandDrawSw(
    const SyncCompositorDemandDrawSwParams& params) {
  DCHECK(compositor_frame_sink_);
  DCHECK(software_draw_shm_->zeroed);
  software_draw_shm_->zeroed = false;

  SkImageInfo info =
      SkImageInfo::MakeN32Premul(params.size.width(), params.size.height());
  size_t stride = info.minRowBytes();
  size_t buffer_size = info.getSafeSize(stride);
  DCHECK_EQ(software_draw_shm_->buffer_size, buffer_size);

  SkBitmap bitmap;
  if (!bitmap.installPixels(info, software_draw_shm_->shm.memory(), stride))
    return;
  SkCanvas canvas(bitmap);
  canvas.clipRect(gfx::RectToSkRect(params.clip));
  canvas.concat(params.transform.matrix());

  compositor_frame_sink_->DemandDrawSw(&canvas);
}

void SynchronousCompositorProxy::SubmitCompositorFrameSw(
    cc::CompositorFrame frame) {
  DCHECK(inside_receive_);
  DCHECK(software_draw_reply_);
  SendDemandDrawSwReply(true, std::move(frame), software_draw_reply_);
  inside_receive_ = false;
}

void SynchronousCompositorProxy::SendDemandDrawSwReply(
    bool success,
    cc::CompositorFrame frame,
    IPC::Message* reply_message) {
  SyncCompositorCommonRendererParams common_renderer_params;
  PopulateCommonParams(&common_renderer_params);
  SyncCompositorMsg_DemandDrawSw::WriteReplyParams(
      reply_message, success, common_renderer_params, frame);
  Send(reply_message);
}

void SynchronousCompositorProxy::SubmitCompositorFrame(
    uint32_t compositor_frame_sink_id,
    cc::CompositorFrame frame) {
  // Verify that exactly one of these is true.
  DCHECK(hardware_draw_reply_async_ || hardware_draw_reply_ ||
         software_draw_reply_);
  DCHECK(!((hardware_draw_reply_ && software_draw_reply_) ||
           (hardware_draw_reply_ && hardware_draw_reply_async_) ||
           (software_draw_reply_ && hardware_draw_reply_async_)));
  if (hardware_draw_reply_async_) {
    SubmitCompositorFrameHwAsync(compositor_frame_sink_id, std::move(frame));
  } else if (hardware_draw_reply_) {
    SubmitCompositorFrameHw(compositor_frame_sink_id, std::move(frame));
  } else if (software_draw_reply_) {
    SubmitCompositorFrameSw(std::move(frame));
  }
}

void SynchronousCompositorProxy::OnComputeScroll(
    base::TimeTicks animation_time) {
  if (need_animate_scroll_) {
    need_animate_scroll_ = false;
    input_handler_proxy_->SynchronouslyAnimate(animation_time);
  }
}

void SynchronousCompositorProxy::SynchronouslyZoomBy(
    float zoom_delta,
    const gfx::Point& anchor,
    SyncCompositorCommonRendererParams* common_renderer_params) {
  DCHECK(!inside_receive_);
  base::AutoReset<bool> scoped_inside_receive(&inside_receive_, true);
  input_handler_proxy_->SynchronouslyZoomBy(zoom_delta, anchor);
  PopulateCommonParams(common_renderer_params);
}

void SynchronousCompositorProxy::SetScroll(
    const gfx::ScrollOffset& new_total_scroll_offset) {
  if (total_scroll_offset_ == new_total_scroll_offset)
    return;
  total_scroll_offset_ = new_total_scroll_offset;
  input_handler_proxy_->SynchronouslySetRootScrollOffset(total_scroll_offset_);
}

}  // namespace content
