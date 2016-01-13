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
//   2. Capture: A bitmap is snapshotted/copied from the RenderView's backing
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

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "content/browser/media/capture/content_video_capture_device_core.h"
#include "content/browser/media/capture/video_capture_oracle.h"
#include "content/browser/media/capture/web_contents_capture_util.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/render_widget_host_view_frame_subscriber.h"
#include "content/public/browser/web_contents_observer.h"
#include "media/base/video_util.h"
#include "media/video/capture/video_capture_types.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace content {

namespace {

// Compute a letterbox region, aligned to even coordinates.
gfx::Rect ComputeYV12LetterboxRegion(const gfx::Size& frame_size,
                                     const gfx::Size& content_size) {

  gfx::Rect result = media::ComputeLetterboxRegion(gfx::Rect(frame_size),
                                                   content_size);

  result.set_x(MakeEven(result.x()));
  result.set_y(MakeEven(result.y()));
  result.set_width(std::max(kMinFrameWidth, MakeEven(result.width())));
  result.set_height(std::max(kMinFrameHeight, MakeEven(result.height())));

  return result;
}

void DeleteOnWorkerThread(scoped_ptr<base::Thread> render_thread,
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
  FrameSubscriber(VideoCaptureOracle::Event event_type,
                  const scoped_refptr<ThreadSafeCaptureOracle>& oracle,
                  VideoFrameDeliveryLog* delivery_log)
      : event_type_(event_type),
        oracle_proxy_(oracle),
        delivery_log_(delivery_log) {}

  virtual bool ShouldCaptureFrame(
      base::TimeTicks present_time,
      scoped_refptr<media::VideoFrame>* storage,
      RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback*
          deliver_frame_cb) OVERRIDE;

 private:
  const VideoCaptureOracle::Event event_type_;
  scoped_refptr<ThreadSafeCaptureOracle> oracle_proxy_;
  VideoFrameDeliveryLog* const delivery_log_;
};

// ContentCaptureSubscription is the relationship between a RenderWidgetHost
// whose content is updating, a subscriber that is deciding which of these
// updates to capture (and where to deliver them to), and a callback that
// knows how to do the capture and prepare the result for delivery.
//
// In practice, this means (a) installing a RenderWidgetHostFrameSubscriber in
// the RenderWidgetHostView, to process updates that occur via accelerated
// compositing, (b) installing itself as an observer of updates to the
// RenderWidgetHost's backing store, to hook updates that occur via software
// rendering, and (c) running a timer to possibly initiate non-event-driven
// captures that the subscriber might request.
//
// All of this happens on the UI thread, although the
// RenderWidgetHostViewFrameSubscriber we install may be dispatching updates
// autonomously on some other thread.
class ContentCaptureSubscription : public content::NotificationObserver {
 public:
  typedef base::Callback<
      void(const base::TimeTicks&,
           const scoped_refptr<media::VideoFrame>&,
           const RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback&)>
      CaptureCallback;

  // Create a subscription. Whenever a manual capture is required, the
  // subscription will invoke |capture_callback| on the UI thread to do the
  // work.
  ContentCaptureSubscription(
      const RenderWidgetHost& source,
      const scoped_refptr<ThreadSafeCaptureOracle>& oracle_proxy,
      const CaptureCallback& capture_callback);
  virtual ~ContentCaptureSubscription();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  void OnTimer();

  const int render_process_id_;
  const int render_view_id_;

  VideoFrameDeliveryLog delivery_log_;
  FrameSubscriber paint_subscriber_;
  FrameSubscriber timer_subscriber_;
  content::NotificationRegistrar registrar_;
  CaptureCallback capture_callback_;
  base::Timer timer_;

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
void RenderVideoFrame(const SkBitmap& input,
                      const scoped_refptr<media::VideoFrame>& output,
                      const base::Callback<void(bool)>& done_cb);

// Keeps track of the RenderView to be sourced, and executes copying of the
// backing store on the UI BrowserThread.
//
// TODO(nick): It would be nice to merge this with WebContentsTracker, but its
// implementation is currently asynchronous -- in our case, the "rvh changed"
// notification would get posted back to the UI thread and processed later, and
// this seems disadvantageous.
class WebContentsCaptureMachine
    : public VideoCaptureMachine,
      public WebContentsObserver {
 public:
  WebContentsCaptureMachine(int render_process_id, int render_view_id);
  virtual ~WebContentsCaptureMachine();

  // VideoCaptureMachine overrides.
  virtual bool Start(const scoped_refptr<ThreadSafeCaptureOracle>& oracle_proxy,
                     const media::VideoCaptureParams& params) OVERRIDE;
  virtual void Stop(const base::Closure& callback) OVERRIDE;

  // Starts a copy from the backing store or the composited surface. Must be run
  // on the UI BrowserThread. |deliver_frame_cb| will be run when the operation
  // completes. The copy will occur to |target|.
  //
  // This may be used as a ContentCaptureSubscription::CaptureCallback.
  void Capture(const base::TimeTicks& start_time,
               const scoped_refptr<media::VideoFrame>& target,
               const RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback&
                   deliver_frame_cb);

  // content::WebContentsObserver implementation.
  virtual void DidShowFullscreenWidget(int routing_id) OVERRIDE {
    fullscreen_widget_id_ = routing_id;
    RenewFrameSubscription();
  }

  virtual void DidDestroyFullscreenWidget(int routing_id) OVERRIDE {
    DCHECK_EQ(fullscreen_widget_id_, routing_id);
    fullscreen_widget_id_ = MSG_ROUTING_NONE;
    RenewFrameSubscription();
  }

  virtual void RenderViewReady() OVERRIDE {
    RenewFrameSubscription();
  }

  virtual void AboutToNavigateRenderView(RenderViewHost* rvh) OVERRIDE {
    RenewFrameSubscription();
  }

  virtual void DidNavigateMainFrame(
      const LoadCommittedDetails& details,
      const FrameNavigateParams& params) OVERRIDE {
    RenewFrameSubscription();
  }

  virtual void WebContentsDestroyed() OVERRIDE;

 private:
  // Starts observing the web contents, returning false if lookup fails.
  bool StartObservingWebContents();

  // Helper function to determine the view that we are currently tracking.
  RenderWidgetHost* GetTarget();

  // Response callback for RenderWidgetHost::CopyFromBackingStore().
  void DidCopyFromBackingStore(
      const base::TimeTicks& start_time,
      const scoped_refptr<media::VideoFrame>& target,
      const RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback&
          deliver_frame_cb,
      bool success,
      const SkBitmap& bitmap);

  // Response callback for RWHVP::CopyFromCompositingSurfaceToVideoFrame().
  void DidCopyFromCompositingSurfaceToVideoFrame(
      const base::TimeTicks& start_time,
      const RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback&
          deliver_frame_cb,
      bool success);

  // Remove the old subscription, and start a new one. This should be called
  // after any change to the WebContents that affects the RenderWidgetHost or
  // attached views.
  void RenewFrameSubscription();

  // Parameters saved in constructor.
  const int initial_render_process_id_;
  const int initial_render_view_id_;

  // A dedicated worker thread on which SkBitmap->VideoFrame conversion will
  // occur. Only used when this activity cannot be done on the GPU.
  scoped_ptr<base::Thread> render_thread_;

  // Makes all the decisions about which frames to copy, and how.
  scoped_refptr<ThreadSafeCaptureOracle> oracle_proxy_;

  // Video capture parameters that this machine is started with.
  media::VideoCaptureParams capture_params_;

  // Routing ID of any active fullscreen render widget or MSG_ROUTING_NONE
  // otherwise.
  int fullscreen_widget_id_;

  // Last known RenderView size.
  gfx::Size last_view_size_;

  // Responsible for forwarding events from the active RenderWidgetHost to the
  // oracle, and initiating captures accordingly.
  scoped_ptr<ContentCaptureSubscription> subscription_;

  // Weak pointer factory used to invalidate callbacks.
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<WebContentsCaptureMachine> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsCaptureMachine);
};

bool FrameSubscriber::ShouldCaptureFrame(
    base::TimeTicks present_time,
    scoped_refptr<media::VideoFrame>* storage,
    DeliverFrameCallback* deliver_frame_cb) {
  TRACE_EVENT1("mirroring", "FrameSubscriber::ShouldCaptureFrame",
               "instance", this);

  ThreadSafeCaptureOracle::CaptureFrameCallback capture_frame_cb;
  bool oracle_decision = oracle_proxy_->ObserveEventAndDecideCapture(
      event_type_, present_time, storage, &capture_frame_cb);

  if (!capture_frame_cb.is_null())
    *deliver_frame_cb = base::Bind(capture_frame_cb, *storage);
  if (oracle_decision)
    delivery_log_->ChronicleFrameDelivery(present_time);
  return oracle_decision;
}

ContentCaptureSubscription::ContentCaptureSubscription(
    const RenderWidgetHost& source,
    const scoped_refptr<ThreadSafeCaptureOracle>& oracle_proxy,
    const CaptureCallback& capture_callback)
    : render_process_id_(source.GetProcess()->GetID()),
      render_view_id_(source.GetRoutingID()),
      delivery_log_(),
      paint_subscriber_(VideoCaptureOracle::kSoftwarePaint, oracle_proxy,
                        &delivery_log_),
      timer_subscriber_(VideoCaptureOracle::kTimerPoll, oracle_proxy,
                        &delivery_log_),
      capture_callback_(capture_callback),
      timer_(true, true) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      source.GetView());

  // Subscribe to accelerated presents. These will be serviced directly by the
  // oracle.
  if (view && kAcceleratedSubscriberIsSupported) {
    scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber(
        new FrameSubscriber(VideoCaptureOracle::kCompositorUpdate,
            oracle_proxy, &delivery_log_));
    view->BeginFrameSubscription(subscriber.Pass());
  }

  // Subscribe to software paint events. This instance will service these by
  // reflecting them back to the WebContentsCaptureMachine via
  // |capture_callback|.
  registrar_.Add(
      this, content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
      Source<RenderWidgetHost>(&source));

  // Subscribe to timer events. This instance will service these as well.
  timer_.Start(FROM_HERE, oracle_proxy->capture_period(),
               base::Bind(&ContentCaptureSubscription::OnTimer,
                          base::Unretained(this)));
}

ContentCaptureSubscription::~ContentCaptureSubscription() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (kAcceleratedSubscriberIsSupported) {
    RenderViewHost* source = RenderViewHost::FromID(render_process_id_,
                                                    render_view_id_);
    if (source) {
      RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
          source->GetView());
      if (view)
        view->EndFrameSubscription();
    }
  }
}

void ContentCaptureSubscription::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE, type);

  RenderWidgetHostImpl* rwh =
      RenderWidgetHostImpl::From(Source<RenderWidgetHost>(source).ptr());

  // This message occurs on window resizes and visibility changes even when
  // accelerated compositing is active, so we need to filter out these cases.
  if (!rwh || !rwh->GetView())
    return;
  // Mac sends DID_UPDATE_BACKING_STORE messages to inform the capture system
  // of new software compositor frames, so always treat these messages as
  // signals of a new frame on Mac.
  // http://crbug.com/333986
#if !defined(OS_MACOSX)
  if (rwh->GetView()->IsSurfaceAvailableForCopy())
    return;
#endif

  TRACE_EVENT1("mirroring", "ContentCaptureSubscription::Observe",
               "instance", this);

  base::Closure copy_done_callback;
  scoped_refptr<media::VideoFrame> frame;
  RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback deliver_frame_cb;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  if (paint_subscriber_.ShouldCaptureFrame(start_time,
                                           &frame,
                                           &deliver_frame_cb)) {
    // This message happens just before paint. If we post a task to do the copy,
    // it should run soon after the paint.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(capture_callback_, start_time, frame, deliver_frame_cb));
  }
}

void ContentCaptureSubscription::OnTimer() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TRACE_EVENT0("mirroring", "ContentCaptureSubscription::OnTimer");

  scoped_refptr<media::VideoFrame> frame;
  RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback deliver_frame_cb;

  const base::TimeTicks start_time = base::TimeTicks::Now();
  if (timer_subscriber_.ShouldCaptureFrame(start_time,
                                           &frame,
                                           &deliver_frame_cb)) {
    capture_callback_.Run(start_time, frame, deliver_frame_cb);
  }
}

void RenderVideoFrame(const SkBitmap& input,
                      const scoped_refptr<media::VideoFrame>& output,
                      const base::Callback<void(bool)>& done_cb) {
  base::ScopedClosureRunner failure_handler(base::Bind(done_cb, false));

  SkAutoLockPixels locker(input);

  // Sanity-check the captured bitmap.
  if (input.empty() ||
      !input.readyToDraw() ||
      input.config() != SkBitmap::kARGB_8888_Config ||
      input.width() < 2 || input.height() < 2) {
    DVLOG(1) << "input unacceptable (size="
             << input.getSize()
             << ", ready=" << input.readyToDraw()
             << ", config=" << input.config() << ')';
    return;
  }

  // Sanity-check the output buffer.
  if (output->format() != media::VideoFrame::I420) {
    NOTREACHED();
    return;
  }

  // Calculate the width and height of the content region in the |output|, based
  // on the aspect ratio of |input|.
  gfx::Rect region_in_frame = ComputeYV12LetterboxRegion(
      output->coded_size(), gfx::Size(input.width(), input.height()));

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

    TRACE_EVENT_ASYNC_STEP_INTO0("mirroring", "Capture", output.get(), "Scale");
    scaled_bitmap = skia::ImageOperations::Resize(input, method,
                                                  region_in_frame.width(),
                                                  region_in_frame.height());
  } else {
    scaled_bitmap = input;
  }

  TRACE_EVENT_ASYNC_STEP_INTO0("mirroring", "Capture", output.get(), "YUV");
  {
    SkAutoLockPixels scaled_bitmap_locker(scaled_bitmap);

    media::CopyRGBToVideoFrame(
        reinterpret_cast<uint8*>(scaled_bitmap.getPixels()),
        scaled_bitmap.rowBytes(),
        region_in_frame,
        output.get());
  }

  // The result is now ready.
  ignore_result(failure_handler.Release());
  done_cb.Run(true);
}

VideoFrameDeliveryLog::VideoFrameDeliveryLog()
    : last_frame_rate_log_time_(),
      count_frames_rendered_(0) {
}

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
      const double measured_fps =
          count_frames_rendered_ / elapsed.InSecondsF();
      UMA_HISTOGRAM_COUNTS(
          "TabCapture.FrameRate",
          static_cast<int>(measured_fps));
      VLOG(1) << "Current measured frame rate for "
              << "WebContentsVideoCaptureDevice is " << measured_fps << " FPS.";
      last_frame_rate_log_time_ = frame_time;
      count_frames_rendered_ = 0;
    }
  }
}

WebContentsCaptureMachine::WebContentsCaptureMachine(int render_process_id,
                                                     int render_view_id)
    : initial_render_process_id_(render_process_id),
      initial_render_view_id_(render_view_id),
      fullscreen_widget_id_(MSG_ROUTING_NONE),
      weak_ptr_factory_(this) {}

WebContentsCaptureMachine::~WebContentsCaptureMachine() {
  BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(&DeleteOnWorkerThread, base::Passed(&render_thread_),
                 base::Bind(&base::DoNothing)));
}

bool WebContentsCaptureMachine::Start(
    const scoped_refptr<ThreadSafeCaptureOracle>& oracle_proxy,
    const media::VideoCaptureParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!started_);

  DCHECK(oracle_proxy.get());
  oracle_proxy_ = oracle_proxy;
  capture_params_ = params;

  render_thread_.reset(new base::Thread("WebContentsVideo_RenderThread"));
  if (!render_thread_->Start()) {
    DVLOG(1) << "Failed to spawn render thread.";
    render_thread_.reset();
    return false;
  }

  if (!StartObservingWebContents()) {
    DVLOG(1) << "Failed to observe web contents.";
    render_thread_.reset();
    return false;
  }

  started_ = true;
  return true;
}

void WebContentsCaptureMachine::Stop(const base::Closure& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  subscription_.reset();
  if (web_contents()) {
    web_contents()->DecrementCapturerCount();
    Observe(NULL);
  }

  // Any callback that intend to use render_thread_ will not work after it is
  // passed.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // The render thread cannot be stopped on the UI thread, so post a message
  // to the thread pool used for blocking operations.
  BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(&DeleteOnWorkerThread, base::Passed(&render_thread_),
                 callback));

  started_ = false;
}

void WebContentsCaptureMachine::Capture(
    const base::TimeTicks& start_time,
    const scoped_refptr<media::VideoFrame>& target,
    const RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback&
        deliver_frame_cb) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RenderWidgetHost* rwh = GetTarget();
  RenderWidgetHostViewBase* view =
      rwh ? static_cast<RenderWidgetHostViewBase*>(rwh->GetView()) : NULL;
  if (!view || !rwh) {
    deliver_frame_cb.Run(base::TimeTicks(), false);
    return;
  }

  gfx::Size video_size = target->coded_size();
  gfx::Size view_size = view->GetViewBounds().size();
  gfx::Size fitted_size;
  if (!view_size.IsEmpty()) {
    fitted_size = ComputeYV12LetterboxRegion(video_size, view_size).size();
  }
  if (view_size != last_view_size_) {
    last_view_size_ = view_size;

    // Measure the number of kilopixels.
    UMA_HISTOGRAM_COUNTS_10000(
        "TabCapture.ViewChangeKiloPixels",
        view_size.width() * view_size.height() / 1024);
  }

  if (view->CanCopyToVideoFrame()) {
    view->CopyFromCompositingSurfaceToVideoFrame(
        gfx::Rect(view_size),
        target,
        base::Bind(&WebContentsCaptureMachine::
                        DidCopyFromCompositingSurfaceToVideoFrame,
                   weak_ptr_factory_.GetWeakPtr(),
                   start_time, deliver_frame_cb));
  } else {
    rwh->CopyFromBackingStore(
        gfx::Rect(),
        fitted_size,  // Size here is a request not always honored.
        base::Bind(&WebContentsCaptureMachine::DidCopyFromBackingStore,
                   weak_ptr_factory_.GetWeakPtr(),
                   start_time,
                   target,
                   deliver_frame_cb),
        SkBitmap::kARGB_8888_Config);
  }
}

bool WebContentsCaptureMachine::StartObservingWebContents() {
  // Look-up the RenderViewHost and, from that, the WebContents that wraps it.
  // If successful, begin observing the WebContents instance.
  //
  // Why this can be unsuccessful: The request for mirroring originates in a
  // render process, and this request is based on the current RenderView
  // associated with a tab.  However, by the time we get up-and-running here,
  // there have been multiple back-and-forth IPCs between processes, as well as
  // a bit of indirection across threads.  It's easily possible that, in the
  // meantime, the original RenderView may have gone away.
  RenderViewHost* const rvh =
      RenderViewHost::FromID(initial_render_process_id_,
                             initial_render_view_id_);
  DVLOG_IF(1, !rvh) << "RenderViewHost::FromID("
                    << initial_render_process_id_ << ", "
                    << initial_render_view_id_ << ") returned NULL.";
  Observe(rvh ? WebContents::FromRenderViewHost(rvh) : NULL);

  WebContentsImpl* contents = static_cast<WebContentsImpl*>(web_contents());
  if (contents) {
    contents->IncrementCapturerCount(oracle_proxy_->GetCaptureSize());
    fullscreen_widget_id_ = contents->GetFullscreenWidgetRoutingID();
    RenewFrameSubscription();
    return true;
  }

  DVLOG(1) << "WebContents::FromRenderViewHost(" << rvh << ") returned NULL.";
  return false;
}

void WebContentsCaptureMachine::WebContentsDestroyed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  subscription_.reset();
  web_contents()->DecrementCapturerCount();
  oracle_proxy_->ReportError("WebContentsDestroyed()");
}

RenderWidgetHost* WebContentsCaptureMachine::GetTarget() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!web_contents())
    return NULL;

  RenderWidgetHost* rwh = NULL;
  if (fullscreen_widget_id_ != MSG_ROUTING_NONE) {
    RenderProcessHost* process = web_contents()->GetRenderProcessHost();
    if (process)
      rwh = RenderWidgetHost::FromID(process->GetID(), fullscreen_widget_id_);
  } else {
    rwh = web_contents()->GetRenderViewHost();
  }

  return rwh;
}

void WebContentsCaptureMachine::DidCopyFromBackingStore(
    const base::TimeTicks& start_time,
    const scoped_refptr<media::VideoFrame>& target,
    const RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback&
        deliver_frame_cb,
    bool success,
    const SkBitmap& bitmap) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::TimeTicks now = base::TimeTicks::Now();
  DCHECK(render_thread_.get());
  if (success) {
    UMA_HISTOGRAM_TIMES("TabCapture.CopyTimeBitmap", now - start_time);
    TRACE_EVENT_ASYNC_STEP_INTO0("mirroring", "Capture", target.get(),
                                 "Render");
    render_thread_->message_loop_proxy()->PostTask(FROM_HERE, base::Bind(
        &RenderVideoFrame, bitmap, target,
        base::Bind(deliver_frame_cb, start_time)));
  } else {
    // Capture can fail due to transient issues, so just skip this frame.
    DVLOG(1) << "CopyFromBackingStore failed; skipping frame.";
    deliver_frame_cb.Run(start_time, false);
  }
}

void WebContentsCaptureMachine::DidCopyFromCompositingSurfaceToVideoFrame(
    const base::TimeTicks& start_time,
    const RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback&
        deliver_frame_cb,
    bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::TimeTicks now = base::TimeTicks::Now();

  if (success) {
    UMA_HISTOGRAM_TIMES("TabCapture.CopyTimeVideoFrame", now - start_time);
  } else {
    // Capture can fail due to transient issues, so just skip this frame.
    DVLOG(1) << "CopyFromCompositingSurface failed; skipping frame.";
  }
  deliver_frame_cb.Run(start_time, success);
}

void WebContentsCaptureMachine::RenewFrameSubscription() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Always destroy the old subscription before creating a new one.
  subscription_.reset();

  RenderWidgetHost* rwh = GetTarget();
  if (!rwh || !rwh->GetView())
    return;

  subscription_.reset(new ContentCaptureSubscription(*rwh, oracle_proxy_,
      base::Bind(&WebContentsCaptureMachine::Capture,
                 weak_ptr_factory_.GetWeakPtr())));
}

}  // namespace

WebContentsVideoCaptureDevice::WebContentsVideoCaptureDevice(
    int render_process_id, int render_view_id)
    : core_(new ContentVideoCaptureDeviceCore(scoped_ptr<VideoCaptureMachine>(
        new WebContentsCaptureMachine(render_process_id, render_view_id)))) {}

WebContentsVideoCaptureDevice::~WebContentsVideoCaptureDevice() {
  DVLOG(2) << "WebContentsVideoCaptureDevice@" << this << " destroying.";
}

// static
media::VideoCaptureDevice* WebContentsVideoCaptureDevice::Create(
    const std::string& device_id) {
  // Parse device_id into render_process_id and render_view_id.
  int render_process_id = -1;
  int render_view_id = -1;
  if (!WebContentsCaptureUtil::ExtractTabCaptureTarget(
           device_id, &render_process_id, &render_view_id)) {
    return NULL;
  }

  return new WebContentsVideoCaptureDevice(render_process_id, render_view_id);
}

void WebContentsVideoCaptureDevice::AllocateAndStart(
    const media::VideoCaptureParams& params,
    scoped_ptr<Client> client) {
  DVLOG(1) << "Allocating " << params.requested_format.frame_size.ToString();
  core_->AllocateAndStart(params, client.Pass());
}

void WebContentsVideoCaptureDevice::StopAndDeAllocate() {
  core_->StopAndDeAllocate();
}

}  // namespace content
