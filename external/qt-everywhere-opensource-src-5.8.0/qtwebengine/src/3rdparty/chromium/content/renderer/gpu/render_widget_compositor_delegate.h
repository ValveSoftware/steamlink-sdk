// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_RENDER_WIDGET_COMPOSITOR_DELEGATE_H_
#define CONTENT_RENDERER_GPU_RENDER_WIDGET_COMPOSITOR_DELEGATE_H_

namespace blink {
class WebWidget;
struct WebScreenInfo;
}

namespace cc {
class BeginFrameSource;
class OutputSurface;
}

namespace content {

// Consumers of RenderWidgetCompositor implement this delegate in order to
// transport compositing information across processes.
class CONTENT_EXPORT RenderWidgetCompositorDelegate {
 public:
  // Report viewport related properties during a commit from the compositor
  // thread.
  virtual void ApplyViewportDeltas(
      const gfx::Vector2dF& inner_delta,
      const gfx::Vector2dF& outer_delta,
      const gfx::Vector2dF& elastic_overscroll_delta,
      float page_scale,
      float top_controls_delta) = 0;

  // Notifies that the compositor has issed a BeginMainFrame.
  virtual void BeginMainFrame(double frame_time_sec) = 0;

  // Requests an OutputSurface to render into.
  virtual std::unique_ptr<cc::OutputSurface> CreateOutputSurface(
      bool fallback) = 0;

  // Requests an external BeginFrameSource from the delegate.
  virtual std::unique_ptr<cc::BeginFrameSource>
  CreateExternalBeginFrameSource() = 0;

  // Notifies that the draw commands for a committed frame have been issued.
  virtual void DidCommitAndDrawCompositorFrame() = 0;

  // Notifies about a compositor frame commit operation having finished.
  virtual void DidCommitCompositorFrame() = 0;

  // Called by the compositor when page scale animation completed.
  virtual void DidCompletePageScaleAnimation() = 0;

  // Notifies that the compositor has posted a swapbuffers operation to the GPU
  // process.
  virtual void DidCompleteSwapBuffers() = 0;

  // Called by the compositor to forward a proto that represents serialized
  // compositor state.
  virtual void ForwardCompositorProto(const std::vector<uint8_t>& proto) = 0;

  // Indicates whether the RenderWidgetCompositor is about to close.
  virtual bool IsClosing() const = 0;

  // Called by the compositor in single-threaded mode when a swap is aborted.
  virtual void OnSwapBuffersAborted() = 0;

  // Called by the compositor in single-threaded mode when a swap completes.
  virtual void OnSwapBuffersComplete() = 0;

  // Called by the compositor in single-threaded mode when a swap is posted.
  virtual void OnSwapBuffersPosted() = 0;

  // Requests that the client schedule a composite now, and calculate
  // appropriate delay for potential future frame.
  virtual void RequestScheduleAnimation() = 0;

  // Requests a visual frame-based update to the state of the delegate if there
  // an update available.
  virtual void UpdateVisualState() = 0;

  // Indicates that the compositor is about to begin a frame. This is primarily
  // to signal to flow control mechanisms that a frame is beginning, not to
  // perform actual painting work.
  virtual void WillBeginCompositorFrame() = 0;

 protected:
  virtual ~RenderWidgetCompositorDelegate() {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_RENDER_WIDGET_COMPOSITOR_DELEGATE_H_
