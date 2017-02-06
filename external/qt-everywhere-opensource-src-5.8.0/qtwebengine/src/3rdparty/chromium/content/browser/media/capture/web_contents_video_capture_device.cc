// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation notes: This needs to work on a variety of hardware
// configurations where the speed of the CPU and GPU greatly affect overall
// performance. Spanning several threads, the process of capturing has been
// split up into four conceptual stages:
//
//   1. Reserve Buffer: Before a frame can be captured, a slot in the client's
//      shared-memory IPC buffer is reserved. There are only a few of these;
//      when they run out, it indicates that the downstream client -- likely a
//      video encoder -- is the performance bottleneck, and that the rate of
//      frame capture should be throttled back.
//
//   2. Capture: A bitmap is snapshotted/copied from the RenderWidget's backing
//      store. This is initiated on the UI BrowserThread, and often occurs
//      asynchronously. Where supported, the GPU scales and color converts
//      frames to our desired size, and the readback happens directly into the
//      shared-memory buffer. But this is not always possible, particularly when
//      accelerated compositing is disabled.
//
//   3. Render (if needed): If the web contents cannot be captured directly into
//      our target size and color format, scaling and colorspace conversion must
//      be done on the CPU. A dedicated thread is used for this operation, to
//      avoid blocking the UI thread. The Render stage always reads from a
//      bitmap returned by Capture, and writes into the reserved slot in the
//      shared-memory buffer.
//
//   4. Deliver: The rendered video frame is returned to the client (which
//      implements the VideoCaptureDevice::Client interface). Because all
//      paths have written the frame into the IPC buffer, this step should
//      never need to do an additional copy of the pixel data.
//
// In the best-performing case, the Render step is bypassed: Capture produces
// ready-to-Deliver frames. But when accelerated readback is not possible, the
// system is designed so that Capture and Render may run concurrently. A timing
// diagram helps illustrate this point (@30 FPS):
//
//    Time: 0ms                 33ms                 66ms                 99ms
// thread1: |-Capture-f1------v |-Capture-f2------v  |-Capture-f3----v    |-Capt
// thread2:                   |-Render-f1-----v   |-Render-f2-----v  |-Render-f3
//
// In the above example, both capturing and rendering *each* take almost the
// full 33 ms available between frames, yet we see that the required throughput
// is obtained.
//
// Turning on verbose logging will cause the effective frame rate to be logged
// at 5-second intervals.

#include "content/browser/media/capture/web_contents_video_capture_device.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/media/capture/cursor_renderer.h"
#include "content/browser/media/capture/web_contents_tracker.h"
#include "content/browser/media/capture/window_activity_tracker.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/render_widget_host_view_frame_subscriber.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_capture_types.h"
#include "media/base/video_frame_metadata.h"
#include "media/base/video_util.h"
#include "media/capture/content/screen_capture_device_core.h"
#include "media/capture/content/thread_safe_capture_oracle.h"
#include "media/capture/content/video_capture_oracle.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/layout.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace content {

namespace {

enum InteractiveModeSettings {
  // Minimum amount of time for which there should be no animation detected
  // to consider interactive mode being active. This is to prevent very brief
  // periods of animated content not being detected (due to CPU fluctations for
  // example) from causing a flip-flop on interactive mode.
  kMinPeriodNoAnimationMillis = 3000
};

void DeleteOnWorkerThread(std::unique_ptr<base::Thread> render_thread,
                          const base::Closure& callback) {
  render_thread.reset();

  // After thread join call the callback on UI thread.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback);
}

// Responsible for logging the effective frame rate.
class VideoFrameDeliveryLog {
 public:
  VideoFrameDeliveryLog();

  // Report that the frame posted with |frame_time| has been delivered.
  void ChronicleFrameDelivery(base::TimeTicks frame_time);

 private:
  // The following keep track of and log the effective frame rate whenever
  // verbose logging is turned on.
  base::TimeTicks last_frame_rate_log_time_;
  int count_frames_rendered_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameDeliveryLog);
};

// FrameSubscriber is a proxy to the ThreadSafeCaptureOracle that's compatible
// with RenderWidgetHostViewFrameSubscriber. We create one per event type.
class FrameSubscriber : public RenderWidgetHostViewFrameSubscriber {
 public:
  FrameSubscriber(media::VideoCaptureOracle::Event event_type,
                  const scoped_refptr<media::ThreadSafeCaptureOracle>& oracle,
                  VideoFrameDeliveryLog* delivery_log,
                  base::WeakPtr<content::CursorRenderer> cursor_renderer,
                  base::WeakPtr<content::WindowActivityTracker> tracker)
      : event_type_(event_type),
        oracle_proxy_(oracle),
        delivery_log_(delivery_log),
        cursor_renderer_(cursor_renderer),
        window_activity_tracker_(tracker),
        weak_ptr_factory_(this) {}

  bool ShouldCaptureFrame(
      const gfx::Rect& damage_rect,
      base::TimeTicks present_time,
      scoped_refptr<media::VideoFrame>* storage,
      RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback*
          deliver_frame_cb) override;

  static void DidCaptureFrame(
      base::WeakPtr<FrameSubscriber> frame_subscriber_,
      const media::ThreadSafeCaptureOracle::CaptureFrameCallback&
          capture_frame_cb,
      const scoped_refptr<media::VideoFrame>& frame,
      base::TimeTicks timestamp,
      const gfx::Rect& region_in_frame,
      bool success);

  bool IsUserInteractingWithContent();

 private:
  const media::VideoCaptureOracle::Event event_type_;
  scoped_refptr<media::ThreadSafeCaptureOracle> oracle_proxy_;
  VideoFrameDeliveryLog* const delivery_log_;
  // We need a weak pointer since FrameSubscriber is owned externally and
  // may outlive the cursor renderer.
  base::WeakPtr<CursorRenderer> cursor_renderer_;
  // We need a weak pointer since FrameSubscriber is owned externally and
  // may outlive the ui activity tracker.
  base::WeakPtr<WindowActivityTracker> window_activity_tracker_;
  base::WeakPtrFactory<FrameSubscriber> weak_ptr_factory_;
};

// ContentCaptureSubscription is the relationship between a RenderWidgetHost
// whose content is updating, a subscriber that is deciding which of these
// updates to capture (and where to deliver them to), and a callback that
// knows how to do the capture and prepare the result for delivery.
//
// In practice, this means (a) installing a RenderWidgetHostFrameSubscriber in
// the RenderWidgetHostView, to process compositor updates, and (b) occasionally
// initiating forced, non-event-driven captures needed by downstream consumers
// that request "refresh frames" of unchanged content.
//
// All of this happens on the UI thread, although the
// RenderWidgetHostViewFrameSubscriber we install may be dispatching updates
// autonomously on some other thread.
class ContentCaptureSubscription {
 public:
  typedef base::Callback<void(
      const base::TimeTicks&,
      const scoped_refptr<media::VideoFrame>&,
      const RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback&)>
      CaptureCallback;

  // Create a subscription. Whenever a manual capture is required, the
  // subscription will invoke |capture_callback| on the UI thread to do the
  // work.
  ContentCaptureSubscription(
      const RenderWidgetHost& source,
      const scoped_refptr<media::ThreadSafeCaptureOracle>& oracle_proxy,
      const CaptureCallback& capture_callback);
  ~ContentCaptureSubscription();

  void MaybeCaptureForRefresh();

 private:
  // Called for active frame refresh requests, or mouse activity events.
  void OnEvent(FrameSubscriber* subscriber);

  // Maintain a weak reference to the RenderWidgetHost (via its routing ID),
  // since the instance could be destroyed externally during the lifetime of
  // |this|.
  const int render_process_id_;
  const int render_widget_id_;

  VideoFrameDeliveryLog delivery_log_;
  std::unique_ptr<FrameSubscriber> refresh_subscriber_;
  std::unique_ptr<FrameSubscriber> mouse_activity_subscriber_;
  CaptureCallback capture_callback_;

  // Responsible for tracking the cursor state and input events to make
  // decisions and then render the mouse cursor on the video frame after
  // capture is completed.
  std::unique_ptr<content::CursorRenderer> cursor_renderer_;

  // Responsible for tracking the UI events and making a decision on whether
  // user is actively interacting with content.
  std::unique_ptr<content::WindowActivityTracker> window_activity_tracker_;

  DISALLOW_COPY_AND_ASSIGN(ContentCaptureSubscription);
};

// Render the SkBitmap |input| into the given VideoFrame buffer |output|, then
// invoke |done_cb| to indicate success or failure. |input| is expected to be
// ARGB. |output| must be YV12 or I420. Colorspace conversion is always done.
// Scaling and letterboxing will be done as needed.
//
// This software implementation should be used only when GPU acceleration of
// these activities is not possible. This operation may be expensive (tens to
// hundreds of milliseconds), so the caller should ensure that it runs on a
// thread where such a pause would cause UI jank.
void RenderVideoFrame(
    const SkBitmap& input,
    const scoped_refptr<media::VideoFrame>& output,
    const base::Callback<void(const gfx::Rect&, bool)>& done_cb);

// Renews capture subscriptions based on feedback from WebContentsTracker, and
// also executes copying of the backing store on the UI BrowserThread.
class WebContentsCaptureMachine : public media::VideoCaptureMachine {
 public:
  WebContentsCaptureMachine(int render_process_id,
                            int main_render_frame_id,
                            bool enable_auto_throttling);
  ~WebContentsCaptureMachine() override;

  // VideoCaptureMachine overrides.
  void Start(const scoped_refptr<media::ThreadSafeCaptureOracle>& oracle_proxy,
             const media::VideoCaptureParams& params,
             const base::Callback<void(bool)> callback) override;
  void Stop(const base::Closure& callback) override;
  bool IsAutoThrottlingEnabled() const override {
    return auto_throttling_enabled_;
  }
  void MaybeCaptureForRefresh() override;

  // Starts a copy from the backing store or the composited surface. Must be run
  // on the UI BrowserThread. |deliver_frame_cb| will be run when the operation
  // completes. The copy will occur to |target|.
  //
  // This may be used as a ContentCaptureSubscription::CaptureCallback.
  void Capture(const base::TimeTicks& start_time,
               const scoped_refptr<media::VideoFrame>& target,
               const RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback&
                   deliver_frame_cb);

 private:
  bool InternalStart(
      const scoped_refptr<media::ThreadSafeCaptureOracle>& oracle_proxy,
      const media::VideoCaptureParams& params);
  void InternalStop(const base::Closure& callback);
  void InternalMaybeCaptureForRefresh();
  bool IsStarted() const;

  // Computes the preferred size of the target RenderWidget for optimal capture.
  gfx::Size ComputeOptimalViewSize() const;

  // Response callback for RenderWidgetHost::CopyFromBackingStore().
  void DidCopyFromBackingStore(
      const base::TimeTicks& start_time,
      const scoped_refptr<media::VideoFrame>& target,
      const RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback&
          deliver_frame_cb,
      const SkBitmap& bitmap,
      ReadbackResponse response);

  // Response callback for RWHVP::CopyFromCompositingSurfaceToVideoFrame().
  void DidCopyFromCompositingSurfaceToVideoFrame(
      const base::TimeTicks& start_time,
      const RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback&
          deliver_frame_cb,
      const gfx::Rect& region_in_frame,
      bool success);

  // Remove the old subscription, and attempt to start a new one if |had_target|
  // is true.
  void RenewFrameSubscription(bool had_target);

  // Called whenever the render widget is resized.
  void UpdateCaptureSize();

  // Parameters saved in constructor.
  const int initial_render_process_id_;
  const int initial_main_render_frame_id_;

  // Tracks events and calls back to RenewFrameSubscription() to maintain
  // capture on the correct RenderWidgetHost.
  const scoped_refptr<WebContentsTracker> tracker_;

  // Set to false to prevent the capture size from automatically adjusting in
  // response to end-to-end utilization.  This is enabled via the throttling
  // option in the WebContentsVideoCaptureDevice device ID.
  const bool auto_throttling_enabled_;

  // A dedicated worker thread on which SkBitmap->VideoFrame conversion will
  // occur. Only used when this activity cannot be done on the GPU.
  std::unique_ptr<base::Thread> render_thread_;

  // Makes all the decisions about which frames to copy, and how.
  scoped_refptr<media::ThreadSafeCaptureOracle> oracle_proxy_;

  // Video capture parameters that this machine is started with.
  media::VideoCaptureParams capture_params_;

  // Last known RenderView size.
  gfx::Size last_view_size_;

  // Responsible for forwarding events from the active RenderWidgetHost to the
  // oracle, and initiating captures accordingly.
  std::unique_ptr<ContentCaptureSubscription> subscription_;

  // Weak pointer factory used to invalidate callbacks.
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<WebContentsCaptureMachine> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsCaptureMachine);
};

bool FrameSubscriber::ShouldCaptureFrame(
    const gfx::Rect& damage_rect,
    base::TimeTicks present_time,
    scoped_refptr<media::VideoFrame>* storage,
    DeliverFrameCallback* deliver_frame_cb) {
  TRACE_EVENT1("gpu.capture", "FrameSubscriber::ShouldCaptureFrame", "instance",
               this);

  media::ThreadSafeCaptureOracle::CaptureFrameCallback capture_frame_cb;

  bool oracle_decision = oracle_proxy_->ObserveEventAndDecideCapture(
      event_type_, damage_rect, present_time, storage, &capture_frame_cb);

  if (!capture_frame_cb.is_null())
    *deliver_frame_cb =
        base::Bind(&FrameSubscriber::DidCaptureFrame,
                   weak_ptr_factory_.GetWeakPtr(), capture_frame_cb, *storage);
  if (oracle_decision)
    delivery_log_->ChronicleFrameDelivery(present_time);
  return oracle_decision;
}

void FrameSubscriber::DidCaptureFrame(
    base::WeakPtr<FrameSubscriber> frame_subscriber_,
    const media::ThreadSafeCaptureOracle::CaptureFrameCallback&
        capture_frame_cb,
    const scoped_refptr<media::VideoFrame>& frame,
    base::TimeTicks timestamp,
    const gfx::Rect& region_in_frame,
    bool success) {
  // We can get a callback in the shutdown sequence for the browser main loop
  // and this can result in a DCHECK failure. Avoid this by doing DCHECK only
  // on success.
  if (success) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // TODO(isheriff): Unclear if taking a snapshot of cursor here affects user
    // experience in any particular scenarios. Doing it prior to capture may
    // require evaluating region_in_frame in this file.
    if (frame_subscriber_ && frame_subscriber_->cursor_renderer_) {
      CursorRenderer* cursor_renderer =
          frame_subscriber_->cursor_renderer_.get();
      if (cursor_renderer->SnapshotCursorState(region_in_frame))
        cursor_renderer->RenderOnVideoFrame(frame);
      frame->metadata()->SetBoolean(
          media::VideoFrameMetadata::INTERACTIVE_CONTENT,
          frame_subscriber_->IsUserInteractingWithContent());
    }
  }
  capture_frame_cb.Run(frame, timestamp, success);
}

bool FrameSubscriber::IsUserInteractingWithContent() {
  bool interactive_mode = false;
  bool ui_activity = false;
  if (window_activity_tracker_) {
    ui_activity = window_activity_tracker_->IsUiInteractionActive();
  }
  if (cursor_renderer_) {
    bool animation_active =
        (base::TimeTicks::Now() -
         oracle_proxy_->last_time_animation_was_detected()) <
        base::TimeDelta::FromMilliseconds(kMinPeriodNoAnimationMillis);
    if (ui_activity && !animation_active) {
      interactive_mode = true;
    } else if (animation_active && window_activity_tracker_.get()) {
      window_activity_tracker_->Reset();
    }
  }
  return interactive_mode;
}

ContentCaptureSubscription::ContentCaptureSubscription(
    const RenderWidgetHost& source,
    const scoped_refptr<media::ThreadSafeCaptureOracle>& oracle_proxy,
    const CaptureCallback& capture_callback)
    : render_process_id_(source.GetProcess()->GetID()),
      render_widget_id_(source.GetRoutingID()),
      delivery_log_(),
      capture_callback_(capture_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderWidgetHostView* const view = source.GetView();
#if defined(USE_AURA) || defined(OS_MACOSX)
  if (view) {
    cursor_renderer_ = CursorRenderer::Create(view->GetNativeView());
    window_activity_tracker_ =
        WindowActivityTracker::Create(view->GetNativeView());
  }
#endif
  refresh_subscriber_.reset(new FrameSubscriber(
      media::VideoCaptureOracle::kActiveRefreshRequest, oracle_proxy,
      &delivery_log_,
      cursor_renderer_ ? cursor_renderer_->GetWeakPtr()
                       : base::WeakPtr<CursorRenderer>(),
      window_activity_tracker_ ? window_activity_tracker_->GetWeakPtr()
                               : base::WeakPtr<WindowActivityTracker>()));
  mouse_activity_subscriber_.reset(new FrameSubscriber(
      media::VideoCaptureOracle::kMouseCursorUpdate, oracle_proxy,
      &delivery_log_, cursor_renderer_ ? cursor_renderer_->GetWeakPtr()
                                       : base::WeakPtr<CursorRenderer>(),
      window_activity_tracker_ ? window_activity_tracker_->GetWeakPtr()
                               : base::WeakPtr<WindowActivityTracker>()));

  // Subscribe to compositor updates. These will be serviced directly by the
  // oracle.
  if (view) {
    std::unique_ptr<RenderWidgetHostViewFrameSubscriber> subscriber(
        new FrameSubscriber(
            media::VideoCaptureOracle::kCompositorUpdate, oracle_proxy,
            &delivery_log_, cursor_renderer_ ? cursor_renderer_->GetWeakPtr()
                                             : base::WeakPtr<CursorRenderer>(),
            window_activity_tracker_ ? window_activity_tracker_->GetWeakPtr()
                                     : base::WeakPtr<WindowActivityTracker>()));
    view->BeginFrameSubscription(std::move(subscriber));
  }

  // Subscribe to mouse movement and mouse cursor update events.
  if (window_activity_tracker_) {
    window_activity_tracker_->RegisterMouseInteractionObserver(
        base::Bind(&ContentCaptureSubscription::OnEvent, base::Unretained(this),
                   mouse_activity_subscriber_.get()));
  }
}

ContentCaptureSubscription::~ContentCaptureSubscription() {
  // If the BrowserThreads have been torn down, then the browser is in the final
  // stages of exiting and it is dangerous to take any further action.  We must
  // return early.  http://crbug.com/396413
  if (!BrowserThread::IsMessageLoopValid(BrowserThread::UI))
    return;

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderWidgetHost* const source =
      RenderWidgetHost::FromID(render_process_id_, render_widget_id_);
  RenderWidgetHostView* const view = source ? source->GetView() : NULL;
  if (view)
    view->EndFrameSubscription();
}

void ContentCaptureSubscription::MaybeCaptureForRefresh() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  OnEvent(refresh_subscriber_.get());
}

void ContentCaptureSubscription::OnEvent(FrameSubscriber* subscriber) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT0("gpu.capture", "ContentCaptureSubscription::OnEvent");

  scoped_refptr<media::VideoFrame> frame;
  RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback deliver_frame_cb;

  const base::TimeTicks start_time = base::TimeTicks::Now();
  DCHECK(subscriber == refresh_subscriber_.get() ||
         subscriber == mouse_activity_subscriber_.get());
  if (subscriber->ShouldCaptureFrame(gfx::Rect(), start_time, &frame,
                                     &deliver_frame_cb)) {
    capture_callback_.Run(start_time, frame, deliver_frame_cb);
  }
}

void RenderVideoFrame(
    const SkBitmap& input,
    const scoped_refptr<media::VideoFrame>& output,
    const base::Callback<void(const gfx::Rect&, bool)>& done_cb) {
  base::ScopedClosureRunner failure_handler(
      base::Bind(done_cb, gfx::Rect(), false));

  SkAutoLockPixels locker(input);

  // Sanity-check the captured bitmap.
  if (input.empty() || !input.readyToDraw() ||
      input.colorType() != kN32_SkColorType || input.width() < 2 ||
      input.height() < 2) {
    DVLOG(1) << "input unacceptable (size=" << input.getSize()
             << ", ready=" << input.readyToDraw()
             << ", colorType=" << input.colorType() << ')';
    return;
  }

  // Sanity-check the output buffer.
  if (output->format() != media::PIXEL_FORMAT_I420) {
    NOTREACHED();
    return;
  }

  // Calculate the width and height of the content region in the |output|, based
  // on the aspect ratio of |input|.
  const gfx::Rect region_in_frame = media::ComputeLetterboxRegion(
      output->visible_rect(), gfx::Size(input.width(), input.height()));

  // Scale the bitmap to the required size, if necessary.
  SkBitmap scaled_bitmap;
  if (input.width() != region_in_frame.width() ||
      input.height() != region_in_frame.height()) {
    skia::ImageOperations::ResizeMethod method;
    if (input.width() < region_in_frame.width() ||
        input.height() < region_in_frame.height()) {
      // Avoid box filtering when magnifying, because it's actually
      // nearest-neighbor.
      method = skia::ImageOperations::RESIZE_HAMMING1;
    } else {
      method = skia::ImageOperations::RESIZE_BOX;
    }

    TRACE_EVENT_ASYNC_STEP_INTO0("gpu.capture", "Capture", output.get(),
                                 "Scale");
    scaled_bitmap = skia::ImageOperations::Resize(
        input, method, region_in_frame.width(), region_in_frame.height());
  } else {
    scaled_bitmap = input;
  }

  TRACE_EVENT_ASYNC_STEP_INTO0("gpu.capture", "Capture", output.get(), "YUV");
  {
    // Align to 2x2 pixel boundaries, as required by
    // media::CopyRGBToVideoFrame().
    const gfx::Rect region_in_yv12_frame(
        region_in_frame.x() & ~1, region_in_frame.y() & ~1,
        region_in_frame.width() & ~1, region_in_frame.height() & ~1);
    if (region_in_yv12_frame.IsEmpty())
      return;

    SkAutoLockPixels scaled_bitmap_locker(scaled_bitmap);
    media::CopyRGBToVideoFrame(
        reinterpret_cast<uint8_t*>(scaled_bitmap.getPixels()),
        scaled_bitmap.rowBytes(), region_in_yv12_frame, output.get());
  }

  // The result is now ready.
  ignore_result(failure_handler.Release());
  done_cb.Run(region_in_frame, true);
}

VideoFrameDeliveryLog::VideoFrameDeliveryLog()
    : last_frame_rate_log_time_(), count_frames_rendered_(0) {}

void VideoFrameDeliveryLog::ChronicleFrameDelivery(base::TimeTicks frame_time) {
  // Log frame rate, if verbose logging is turned on.
  static const base::TimeDelta kFrameRateLogInterval =
      base::TimeDelta::FromSeconds(10);
  if (last_frame_rate_log_time_.is_null()) {
    last_frame_rate_log_time_ = frame_time;
    count_frames_rendered_ = 0;
  } else {
    ++count_frames_rendered_;
    const base::TimeDelta elapsed = frame_time - last_frame_rate_log_time_;
    if (elapsed >= kFrameRateLogInterval) {
      const double measured_fps = count_frames_rendered_ / elapsed.InSecondsF();
      UMA_HISTOGRAM_COUNTS("TabCapture.FrameRate",
                           static_cast<int>(measured_fps));
      VLOG(1) << "Current measured frame rate for "
              << "WebContentsVideoCaptureDevice is " << measured_fps << " FPS.";
      last_frame_rate_log_time_ = frame_time;
      count_frames_rendered_ = 0;
    }
  }
}

WebContentsCaptureMachine::WebContentsCaptureMachine(
    int render_process_id,
    int main_render_frame_id,
    bool enable_auto_throttling)
    : initial_render_process_id_(render_process_id),
      initial_main_render_frame_id_(main_render_frame_id),
      tracker_(new WebContentsTracker(true)),
      auto_throttling_enabled_(enable_auto_throttling),
      weak_ptr_factory_(this) {
  DVLOG(1) << "Created WebContentsCaptureMachine for " << render_process_id
           << ':' << main_render_frame_id
           << (auto_throttling_enabled_ ? " with auto-throttling enabled" : "");
}

WebContentsCaptureMachine::~WebContentsCaptureMachine() {}

bool WebContentsCaptureMachine::IsStarted() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return weak_ptr_factory_.HasWeakPtrs();
}

void WebContentsCaptureMachine::Start(
    const scoped_refptr<media::ThreadSafeCaptureOracle>& oracle_proxy,
    const media::VideoCaptureParams& params,
    const base::Callback<void(bool)> callback) {
  // Starts the capture machine asynchronously.
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WebContentsCaptureMachine::InternalStart,
                 base::Unretained(this), oracle_proxy, params),
      callback);
}

bool WebContentsCaptureMachine::InternalStart(
    const scoped_refptr<media::ThreadSafeCaptureOracle>& oracle_proxy,
    const media::VideoCaptureParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!IsStarted());

  DCHECK(oracle_proxy);
  oracle_proxy_ = oracle_proxy;
  capture_params_ = params;

  render_thread_.reset(new base::Thread("WebContentsVideo_RenderThread"));
  if (!render_thread_->Start()) {
    DVLOG(1) << "Failed to spawn render thread.";
    render_thread_.reset();
    return false;
  }

  // Note: Creation of the first WeakPtr in the following statement will cause
  // IsStarted() to return true from now on.
  tracker_->SetResizeChangeCallback(
      base::Bind(&WebContentsCaptureMachine::UpdateCaptureSize,
                 weak_ptr_factory_.GetWeakPtr()));
  tracker_->Start(initial_render_process_id_, initial_main_render_frame_id_,
                  base::Bind(&WebContentsCaptureMachine::RenewFrameSubscription,
                             weak_ptr_factory_.GetWeakPtr()));

  return true;
}

void WebContentsCaptureMachine::Stop(const base::Closure& callback) {
  // Stops the capture machine asynchronously.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&WebContentsCaptureMachine::InternalStop,
                                     base::Unretained(this), callback));
}

void WebContentsCaptureMachine::InternalStop(const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!IsStarted()) {
    callback.Run();
    return;
  }

  // The following cancels any outstanding callbacks and causes IsStarted() to
  // return false from here onward.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Note: RenewFrameSubscription() must be called before stopping |tracker_| so
  // the web_contents() can be notified that the capturing is ending.
  RenewFrameSubscription(false);
  tracker_->Stop();

  // The render thread cannot be stopped on the UI thread, so post a message
  // to the thread pool used for blocking operations.
  if (render_thread_) {
    BrowserThread::PostBlockingPoolTask(
        FROM_HERE, base::Bind(&DeleteOnWorkerThread,
                              base::Passed(&render_thread_), callback));
  }
}

void WebContentsCaptureMachine::MaybeCaptureForRefresh() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WebContentsCaptureMachine::InternalMaybeCaptureForRefresh,
                 // Use of Unretained() is safe here since this task must run
                 // before InternalStop().
                 base::Unretained(this)));
}

void WebContentsCaptureMachine::InternalMaybeCaptureForRefresh() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (IsStarted() && subscription_)
    subscription_->MaybeCaptureForRefresh();
}

void WebContentsCaptureMachine::Capture(
    const base::TimeTicks& start_time,
    const scoped_refptr<media::VideoFrame>& target,
    const RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback&
        deliver_frame_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderWidgetHost* rwh = tracker_->GetTargetRenderWidgetHost();
  RenderWidgetHostViewBase* view =
      rwh ? static_cast<RenderWidgetHostViewBase*>(rwh->GetView()) : NULL;
  if (!view) {
    deliver_frame_cb.Run(base::TimeTicks(), gfx::Rect(), false);
    return;
  }

  gfx::Size view_size = view->GetViewBounds().size();
  if (view_size != last_view_size_) {
    last_view_size_ = view_size;

    // Measure the number of kilopixels.
    UMA_HISTOGRAM_COUNTS_10000("TabCapture.ViewChangeKiloPixels",
                               view_size.width() * view_size.height() / 1024);
  }

  if (view->CanCopyToVideoFrame()) {
    view->CopyFromCompositingSurfaceToVideoFrame(
        gfx::Rect(view_size), target,
        base::Bind(&WebContentsCaptureMachine::
                       DidCopyFromCompositingSurfaceToVideoFrame,
                   weak_ptr_factory_.GetWeakPtr(), start_time,
                   deliver_frame_cb));
  } else {
    const gfx::Size fitted_size =
        view_size.IsEmpty()
            ? gfx::Size()
            : media::ComputeLetterboxRegion(target->visible_rect(), view_size)
                  .size();
    rwh->CopyFromBackingStore(
        gfx::Rect(),
        fitted_size,  // Size here is a request not always honored.
        base::Bind(&WebContentsCaptureMachine::DidCopyFromBackingStore,
                   weak_ptr_factory_.GetWeakPtr(), start_time, target,
                   deliver_frame_cb),
        kN32_SkColorType);
  }
}

gfx::Size WebContentsCaptureMachine::ComputeOptimalViewSize() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(miu): Propagate capture frame size changes as new "preferred size"
  // updates, rather than just using the max frame size.
  // http://crbug.com/350491
  gfx::Size optimal_size = oracle_proxy_->max_frame_size();

  switch (capture_params_.resolution_change_policy) {
    case media::RESOLUTION_POLICY_FIXED_RESOLUTION:
      break;
    case media::RESOLUTION_POLICY_FIXED_ASPECT_RATIO:
    case media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT: {
      // If the max frame size is close to a common video aspect ratio, compute
      // a standard resolution for that aspect ratio.  For example, given
      // 1365x768, which is very close to 16:9, the optimal size would be
      // 1280x720.  The purpose of this logic is to prevent scaling quality
      // issues caused by "one pixel stretching" and/or odd-to-even dimension
      // scaling, and to improve the performance of consumers of the captured
      // video.
      const auto HasIntendedAspectRatio = [](
          const gfx::Size& size, int width_units, int height_units) {
        const int a = height_units * size.width();
        const int b = width_units * size.height();
        const int percentage_diff = 100 * std::abs((a - b)) / b;
        return percentage_diff <= 1;  // Effectively, anything strictly <2%.
      };
      const auto RoundToExactAspectRatio = [](const gfx::Size& size,
                                              int width_step, int height_step) {
        const int adjusted_height = std::max(
            size.height() - (size.height() % height_step), height_step);
        DCHECK_EQ((adjusted_height * width_step) % height_step, 0);
        return gfx::Size(adjusted_height * width_step / height_step,
                         adjusted_height);
      };
      if (HasIntendedAspectRatio(optimal_size, 16, 9))
        optimal_size = RoundToExactAspectRatio(optimal_size, 160, 90);
      else if (HasIntendedAspectRatio(optimal_size, 4, 3))
        optimal_size = RoundToExactAspectRatio(optimal_size, 64, 48);
      // Else, do not make an adjustment.
      break;
    }
  }

  // If the ratio between physical and logical pixels is greater than 1:1,
  // shrink |optimal_size| by that amount.  Then, when external code resizes the
  // render widget to the "preferred size," the widget will be physically
  // rendered at the exact capture size, thereby eliminating unnecessary scaling
  // operations in the graphics pipeline.
  RenderWidgetHost* const rwh = tracker_->GetTargetRenderWidgetHost();
  RenderWidgetHostView* const rwhv = rwh ? rwh->GetView() : NULL;
  if (rwhv) {
    const gfx::NativeView view = rwhv->GetNativeView();
    const float scale = ui::GetScaleFactorForNativeView(view);
    if (scale > 1.0f) {
      const gfx::Size shrunk_size =
          gfx::ScaleToFlooredSize(optimal_size, 1.0f / scale);
      if (shrunk_size.width() > 0 && shrunk_size.height() > 0)
        optimal_size = shrunk_size;
    }
  }

  VLOG(1) << "Computed optimal target size: " << optimal_size.ToString();
  return optimal_size;
}

void WebContentsCaptureMachine::DidCopyFromBackingStore(
    const base::TimeTicks& start_time,
    const scoped_refptr<media::VideoFrame>& target,
    const RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback&
        deliver_frame_cb,
    const SkBitmap& bitmap,
    ReadbackResponse response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::TimeTicks now = base::TimeTicks::Now();
  DCHECK(render_thread_);
  if (response == READBACK_SUCCESS) {
    UMA_HISTOGRAM_TIMES("TabCapture.CopyTimeBitmap", now - start_time);
    TRACE_EVENT_ASYNC_STEP_INTO0("gpu.capture", "Capture", target.get(),
                                 "Render");
    render_thread_->task_runner()->PostTask(
        FROM_HERE, media::BindToCurrentLoop(
                       base::Bind(&RenderVideoFrame, bitmap, target,
                                  base::Bind(deliver_frame_cb, start_time))));
  } else {
    // Capture can fail due to transient issues, so just skip this frame.
    DVLOG(1) << "CopyFromBackingStore failed; skipping frame.";
    deliver_frame_cb.Run(start_time, gfx::Rect(), false);
  }
}

void WebContentsCaptureMachine::DidCopyFromCompositingSurfaceToVideoFrame(
    const base::TimeTicks& start_time,
    const RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback&
        deliver_frame_cb,
    const gfx::Rect& region_in_frame,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::TimeTicks now = base::TimeTicks::Now();

  if (success) {
    UMA_HISTOGRAM_TIMES("TabCapture.CopyTimeVideoFrame", now - start_time);
  } else {
    // Capture can fail due to transient issues, so just skip this frame.
    DVLOG(1) << "CopyFromCompositingSurface failed; skipping frame.";
  }
  deliver_frame_cb.Run(start_time, region_in_frame, success);
}

void WebContentsCaptureMachine::RenewFrameSubscription(bool had_target) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderWidgetHost* const rwh =
      had_target ? tracker_->GetTargetRenderWidgetHost() : nullptr;

  // Always destroy the old subscription before creating a new one.
  const bool had_subscription = !!subscription_;
  subscription_.reset();

  DVLOG(1) << "Renewing frame subscription to RWH@" << rwh
           << ", had_subscription=" << had_subscription;

  if (!rwh) {
    if (had_subscription && tracker_->web_contents())
      tracker_->web_contents()->DecrementCapturerCount();
    if (IsStarted()) {
      // Tracking of WebContents and/or its main frame has failed before Stop()
      // was called, so report this as an error:
      oracle_proxy_->ReportError(FROM_HERE,
                                 "WebContents and/or main frame are gone.");
    }
    return;
  }

  if (!had_subscription && tracker_->web_contents())
    tracker_->web_contents()->IncrementCapturerCount(ComputeOptimalViewSize());

  subscription_.reset(new ContentCaptureSubscription(
      *rwh, oracle_proxy_, base::Bind(&WebContentsCaptureMachine::Capture,
                                      weak_ptr_factory_.GetWeakPtr())));
}

void WebContentsCaptureMachine::UpdateCaptureSize() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!oracle_proxy_)
    return;
  RenderWidgetHost* const rwh = tracker_->GetTargetRenderWidgetHost();
  RenderWidgetHostView* const view = rwh ? rwh->GetView() : nullptr;
  if (!view)
    return;

  // Convert the view's size from the DIP coordinate space to the pixel
  // coordinate space.  When the view is being rendered on a high-DPI display,
  // this allows the high-resolution image detail to propagate through to the
  // captured video.
  const gfx::Size view_size = view->GetViewBounds().size();
  const gfx::Size physical_size = gfx::ConvertSizeToPixel(
      ui::GetScaleFactorForNativeView(view->GetNativeView()), view_size);
  VLOG(1) << "Computed physical capture size (" << physical_size.ToString()
          << ") from view size (" << view_size.ToString() << ").";

  oracle_proxy_->UpdateCaptureSize(physical_size);
}

}  // namespace

WebContentsVideoCaptureDevice::WebContentsVideoCaptureDevice(
    int render_process_id,
    int main_render_frame_id,
    bool enable_auto_throttling)
    : core_(new media::ScreenCaptureDeviceCore(
          std::unique_ptr<media::VideoCaptureMachine>(
              new WebContentsCaptureMachine(render_process_id,
                                            main_render_frame_id,
                                            enable_auto_throttling)))) {}

WebContentsVideoCaptureDevice::~WebContentsVideoCaptureDevice() {
  DVLOG(2) << "WebContentsVideoCaptureDevice@" << this << " destroying.";
}

// static
media::VideoCaptureDevice* WebContentsVideoCaptureDevice::Create(
    const std::string& device_id) {
  // Parse device_id into render_process_id and main_render_frame_id.
  int render_process_id = -1;
  int main_render_frame_id = -1;
  if (!WebContentsMediaCaptureId::ExtractTabCaptureTarget(
          device_id, &render_process_id, &main_render_frame_id)) {
    return NULL;
  }

  return new WebContentsVideoCaptureDevice(
      render_process_id, main_render_frame_id,
      WebContentsMediaCaptureId::IsAutoThrottlingOptionSet(device_id));
}

void WebContentsVideoCaptureDevice::AllocateAndStart(
    const media::VideoCaptureParams& params,
    std::unique_ptr<Client> client) {
  DVLOG(1) << "Allocating " << params.requested_format.frame_size.ToString();
  core_->AllocateAndStart(params, std::move(client));
}

void WebContentsVideoCaptureDevice::RequestRefreshFrame() {
  core_->RequestRefreshFrame();
}

void WebContentsVideoCaptureDevice::StopAndDeAllocate() {
  core_->StopAndDeAllocate();
}

}  // namespace content
