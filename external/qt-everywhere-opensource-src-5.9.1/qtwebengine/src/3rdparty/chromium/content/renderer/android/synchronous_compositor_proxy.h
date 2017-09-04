// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_PROXY_H_
#define CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_PROXY_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/optional.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/renderer/android/synchronous_compositor_frame_sink.h"
#include "ui/events/blink/synchronous_input_handler_proxy.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/geometry/size_f.h"

namespace IPC {
class Message;
class Sender;
}  // namespace IPC

namespace cc {
class CompositorFrame;
}  // namespace cc

namespace content {

class SynchronousCompositorFrameSink;
struct SyncCompositorCommonRendererParams;
struct SyncCompositorDemandDrawHwParams;
struct SyncCompositorDemandDrawSwParams;
struct SyncCompositorSetSharedMemoryParams;

class SynchronousCompositorProxy : public ui::SynchronousInputHandler,
                                   public SynchronousCompositorFrameSinkClient {
 public:
  SynchronousCompositorProxy(
      int routing_id,
      IPC::Sender* sender,
      ui::SynchronousInputHandlerProxy* input_handler_proxy);
  ~SynchronousCompositorProxy() override;

  // ui::SynchronousInputHandler overrides.
  void SetNeedsSynchronousAnimateInput() override;
  void UpdateRootLayerState(const gfx::ScrollOffset& total_scroll_offset,
                            const gfx::ScrollOffset& max_scroll_offset,
                            const gfx::SizeF& scrollable_size,
                            float page_scale_factor,
                            float min_page_scale_factor,
                            float max_page_scale_factor) override;

  // SynchronousCompositorFrameSinkClient overrides.
  void DidActivatePendingTree() override;
  void Invalidate() override;
  void SubmitCompositorFrame(uint32_t compositor_frame_sink_id,
                             cc::CompositorFrame frame) override;

  void SetCompositorFrameSink(
      SynchronousCompositorFrameSink* compositor_frame_sink);
  void OnMessageReceived(const IPC::Message& message);
  bool Send(IPC::Message* message);
  void PopulateCommonParams(SyncCompositorCommonRendererParams* params) const;

 private:
  struct SharedMemoryWithSize;

  // IPC handlers.
  void OnComputeScroll(base::TimeTicks animation_time);
  void DemandDrawHwAsync(const SyncCompositorDemandDrawHwParams& params);
  void DemandDrawHw(const SyncCompositorDemandDrawHwParams& params,
                    IPC::Message* reply_message);
  void SetSharedMemory(
      const SyncCompositorSetSharedMemoryParams& params,
      bool* success,
      SyncCompositorCommonRendererParams* common_renderer_params);
  void ZeroSharedMemory();
  void DemandDrawSw(const SyncCompositorDemandDrawSwParams& params,
                    IPC::Message* reply_message);
  void SynchronouslyZoomBy(
      float zoom_delta,
      const gfx::Point& anchor,
      SyncCompositorCommonRendererParams* common_renderer_params);
  void SetScroll(const gfx::ScrollOffset& total_scroll_offset);

  void SubmitCompositorFrameHwAsync(uint32_t compositor_frame_sink_id,
                                    cc::CompositorFrame frame);
  void SubmitCompositorFrameHw(uint32_t compositor_frame_sink_id,
                               cc::CompositorFrame frame);
  void SendDemandDrawHwReply(base::Optional<cc::CompositorFrame> frame,
                             uint32_t compositor_frame_sink_id,
                             IPC::Message* reply_message);
  void SendDemandDrawHwReplyAsync(base::Optional<cc::CompositorFrame> frame,
                                  uint32_t compositor_frame_sink_id);
  void DoDemandDrawSw(const SyncCompositorDemandDrawSwParams& params);
  void SubmitCompositorFrameSw(cc::CompositorFrame frame);
  void SendDemandDrawSwReply(bool success,
                             cc::CompositorFrame frame,
                             IPC::Message* reply_message);
  void SendAsyncRendererStateIfNeeded();
  void DoDemandDrawHw(const SyncCompositorDemandDrawHwParams& params,
                      IPC::Message* reply_message);

  const int routing_id_;
  IPC::Sender* const sender_;
  ui::SynchronousInputHandlerProxy* const input_handler_proxy_;
  const bool use_in_process_zero_copy_software_draw_;
  SynchronousCompositorFrameSink* compositor_frame_sink_;
  bool inside_receive_;
  IPC::Message* hardware_draw_reply_;
  IPC::Message* software_draw_reply_;
  bool hardware_draw_reply_async_;

  // From browser.
  std::unique_ptr<SharedMemoryWithSize> software_draw_shm_;

  // To browser.
  mutable uint32_t version_;  // Mustable so PopulateCommonParams can be const.
  gfx::ScrollOffset total_scroll_offset_;  // Modified by both.
  gfx::ScrollOffset max_scroll_offset_;
  gfx::SizeF scrollable_size_;
  float page_scale_factor_;
  float min_page_scale_factor_;
  float max_page_scale_factor_;
  bool need_animate_scroll_;
  uint32_t need_invalidate_count_;
  uint32_t did_activate_pending_tree_count_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorProxy);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_PROXY_H_
