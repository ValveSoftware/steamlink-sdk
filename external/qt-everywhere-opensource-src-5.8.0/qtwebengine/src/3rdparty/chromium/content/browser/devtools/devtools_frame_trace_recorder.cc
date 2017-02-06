// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_frame_trace_recorder.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/atomicops.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/trace_event_impl.h"
#include "cc/output/compositor_frame_metadata.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/browser/readback_types.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace content {

namespace {

static base::subtle::Atomic32 frame_data_count = 0;
static int kMaximumFrameDataCount = 150;
static size_t kFrameAreaLimit = 256000;

class TraceableDevToolsScreenshot
    : public base::trace_event::ConvertableToTraceFormat {
 public:
  TraceableDevToolsScreenshot(const SkBitmap& bitmap) : frame_(bitmap) {}

  void AppendAsTraceFormat(std::string* out) const override {
    out->append("\"");
    if (!frame_.drawsNothing()) {
      std::vector<unsigned char> data;
      SkAutoLockPixels lock_image(frame_);
      bool encoded = gfx::JPEGCodec::Encode(
          reinterpret_cast<unsigned char*>(frame_.getAddr32(0, 0)),
          gfx::JPEGCodec::FORMAT_SkBitmap,
          frame_.width(), frame_.height(),
          frame_.width() * frame_.bytesPerPixel(), 80, &data);
      if (encoded) {
        std::string encoded_data;
        base::Base64Encode(
            base::StringPiece(reinterpret_cast<char*>(&data[0]), data.size()),
            &encoded_data);
        out->append(encoded_data);
      }
    }
    out->append("\"");
  }

 private:
  ~TraceableDevToolsScreenshot() override {
    base::subtle::NoBarrier_AtomicIncrement(&frame_data_count, -1);
  }

  SkBitmap frame_;
};

void FrameCaptured(base::TimeTicks timestamp, const SkBitmap& bitmap,
    ReadbackResponse response) {
  if (response != READBACK_SUCCESS)
    return;
  int current_frame_count = base::subtle::NoBarrier_Load(&frame_data_count);
  if (current_frame_count >= kMaximumFrameDataCount)
    return;
  if (bitmap.drawsNothing())
    return;
  base::subtle::NoBarrier_AtomicIncrement(&frame_data_count, 1);
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID_AND_TIMESTAMP(
      TRACE_DISABLED_BY_DEFAULT("devtools.screenshot"), "Screenshot", 1,
      timestamp.ToInternalValue(),
      std::unique_ptr<base::trace_event::ConvertableToTraceFormat>(
          new TraceableDevToolsScreenshot(bitmap)));
}

void CaptureFrame(RenderFrameHostImpl* host,
    const cc::CompositorFrameMetadata& metadata) {
  RenderWidgetHostViewBase* view =
      static_cast<RenderWidgetHostViewBase*>(host->GetView());
  if (!view)
    return;
  int current_frame_count = base::subtle::NoBarrier_Load(&frame_data_count);
  if (current_frame_count >= kMaximumFrameDataCount)
    return;

  gfx::Size src_size = gfx::ToCeiledSize(gfx::ScaleSize(
      metadata.scrollable_viewport_size, metadata.page_scale_factor));
  gfx::Size snapshot_size;
  float area = src_size.GetArea();
  if (area <= kFrameAreaLimit) {
    snapshot_size = src_size;
  } else {
    double scale = sqrt(kFrameAreaLimit / area);
    snapshot_size = gfx::ScaleToCeiledSize(src_size, scale);
  }

  view->CopyFromCompositingSurface(
      gfx::Rect(gfx::Point(), src_size), snapshot_size,
      base::Bind(FrameCaptured, base::TimeTicks::Now()),
      kN32_SkColorType);
}

bool ScreenshotCategoryEnabled() {
  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("devtools.screenshot"), &enabled);
  return enabled;
}

}  // namespace

DevToolsFrameTraceRecorder::DevToolsFrameTraceRecorder() { }

DevToolsFrameTraceRecorder::~DevToolsFrameTraceRecorder() { }

void DevToolsFrameTraceRecorder::OnSwapCompositorFrame(
    RenderFrameHostImpl* host,
    const cc::CompositorFrameMetadata& frame_metadata) {
  if (!host || !ScreenshotCategoryEnabled())
    return;
  CaptureFrame(host, frame_metadata);
}

void DevToolsFrameTraceRecorder::OnSynchronousSwapCompositorFrame(
    RenderFrameHostImpl* host,
    const cc::CompositorFrameMetadata& frame_metadata) {
  if (!host || !ScreenshotCategoryEnabled()) {
    last_metadata_.reset();
    return;
  }

  bool is_new_trace;
  TRACE_EVENT_IS_NEW_TRACE(&is_new_trace);
  if (!is_new_trace && last_metadata_)
    CaptureFrame(host, *last_metadata_);
  last_metadata_.reset(new cc::CompositorFrameMetadata);
  *last_metadata_ = frame_metadata.Clone();
}

}  // namespace content
