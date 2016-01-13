// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_video_capture_device.h"

#include "base/bind_helpers.h"
#include "base/debug/debugger.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/media/capture/video_capture_oracle.h"
#include "content/browser/media/capture/web_contents_capture_util.h"
#include "content/browser/renderer_host/media/video_capture_buffer_pool.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_view_frame_subscriber.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/base/yuv_convert.h"
#include "media/video/capture/video_capture_types.h"
#include "skia/ext/platform_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace content {
namespace {

const int kTestWidth = 320;
const int kTestHeight = 240;
const int kTestFramesPerSecond = 20;
const SkColor kNothingYet = 0xdeadbeef;
const SkColor kNotInterested = ~kNothingYet;

void DeadlineExceeded(base::Closure quit_closure) {
  if (!base::debug::BeingDebugged()) {
    quit_closure.Run();
    FAIL() << "Deadline exceeded while waiting, quitting";
  } else {
    LOG(WARNING) << "Deadline exceeded; test would fail if debugger weren't "
                 << "attached.";
  }
}

void RunCurrentLoopWithDeadline() {
  base::Timer deadline(false, false);
  deadline.Start(FROM_HERE, TestTimeouts::action_max_timeout(), base::Bind(
      &DeadlineExceeded, base::MessageLoop::current()->QuitClosure()));
  base::MessageLoop::current()->Run();
  deadline.Stop();
}

SkColor ConvertRgbToYuv(SkColor rgb) {
  uint8 yuv[3];
  media::ConvertRGB32ToYUV(reinterpret_cast<uint8*>(&rgb),
                           yuv, yuv + 1, yuv + 2, 1, 1, 1, 1, 1);
  return SkColorSetRGB(yuv[0], yuv[1], yuv[2]);
}

// Thread-safe class that controls the source pattern to be captured by the
// system under test. The lifetime of this class is greater than the lifetime
// of all objects that reference it, so it does not need to be reference
// counted.
class CaptureTestSourceController {
 public:

  CaptureTestSourceController()
      : color_(SK_ColorMAGENTA),
        copy_result_size_(kTestWidth, kTestHeight),
        can_copy_to_video_frame_(false),
        use_frame_subscriber_(false) {}

  void SetSolidColor(SkColor color) {
    base::AutoLock guard(lock_);
    color_ = color;
  }

  SkColor GetSolidColor() {
    base::AutoLock guard(lock_);
    return color_;
  }

  void SetCopyResultSize(int width, int height) {
    base::AutoLock guard(lock_);
    copy_result_size_ = gfx::Size(width, height);
  }

  gfx::Size GetCopyResultSize() {
    base::AutoLock guard(lock_);
    return copy_result_size_;
  }

  void SignalCopy() {
    // TODO(nick): This actually should always be happening on the UI thread.
    base::AutoLock guard(lock_);
    if (!copy_done_.is_null()) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, copy_done_);
      copy_done_.Reset();
    }
  }

  void SetCanCopyToVideoFrame(bool value) {
    base::AutoLock guard(lock_);
    can_copy_to_video_frame_ = value;
  }

  bool CanCopyToVideoFrame() {
    base::AutoLock guard(lock_);
    return can_copy_to_video_frame_;
  }

  void SetUseFrameSubscriber(bool value) {
    base::AutoLock guard(lock_);
    use_frame_subscriber_ = value;
  }

  bool CanUseFrameSubscriber() {
    base::AutoLock guard(lock_);
    return use_frame_subscriber_;
  }

  void WaitForNextCopy() {
    {
      base::AutoLock guard(lock_);
      copy_done_ = base::MessageLoop::current()->QuitClosure();
    }

    RunCurrentLoopWithDeadline();
  }

 private:
  base::Lock lock_;  // Guards changes to all members.
  SkColor color_;
  gfx::Size copy_result_size_;
  bool can_copy_to_video_frame_;
  bool use_frame_subscriber_;
  base::Closure copy_done_;

  DISALLOW_COPY_AND_ASSIGN(CaptureTestSourceController);
};

// A stub implementation which returns solid-color bitmaps in calls to
// CopyFromCompositingSurfaceToVideoFrame(), and which allows the video-frame
// readback path to be switched on and off. The behavior is controlled by a
// CaptureTestSourceController.
class CaptureTestView : public TestRenderWidgetHostView {
 public:
  explicit CaptureTestView(RenderWidgetHostImpl* rwh,
                           CaptureTestSourceController* controller)
      : TestRenderWidgetHostView(rwh),
        controller_(controller) {}

  virtual ~CaptureTestView() {}

  // TestRenderWidgetHostView overrides.
  virtual gfx::Rect GetViewBounds() const OVERRIDE {
    return gfx::Rect(100, 100, 100 + kTestWidth, 100 + kTestHeight);
  }

  virtual bool CanCopyToVideoFrame() const OVERRIDE {
    return controller_->CanCopyToVideoFrame();
  }

  virtual void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) OVERRIDE {
    SkColor c = ConvertRgbToYuv(controller_->GetSolidColor());
    media::FillYUV(
        target.get(), SkColorGetR(c), SkColorGetG(c), SkColorGetB(c));
    callback.Run(true);
    controller_->SignalCopy();
  }

  virtual void BeginFrameSubscription(
      scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber) OVERRIDE {
    subscriber_.reset(subscriber.release());
  }

  virtual void EndFrameSubscription() OVERRIDE {
    subscriber_.reset();
  }

  // Simulate a compositor paint event for our subscriber.
  void SimulateUpdate() {
    const base::TimeTicks present_time = base::TimeTicks::Now();
    RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback callback;
    scoped_refptr<media::VideoFrame> target;
    if (subscriber_ && subscriber_->ShouldCaptureFrame(present_time,
                                                       &target, &callback)) {
      SkColor c = ConvertRgbToYuv(controller_->GetSolidColor());
      media::FillYUV(
          target.get(), SkColorGetR(c), SkColorGetG(c), SkColorGetB(c));
      BrowserThread::PostTask(BrowserThread::UI,
                              FROM_HERE,
                              base::Bind(callback, present_time, true));
      controller_->SignalCopy();
    }
  }

 private:
  scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber_;
  CaptureTestSourceController* const controller_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(CaptureTestView);
};

#if defined(COMPILER_MSVC)
// MSVC warns on diamond inheritance. See comment for same warning on
// RenderViewHostImpl.
#pragma warning(push)
#pragma warning(disable: 4250)
#endif

// A stub implementation which returns solid-color bitmaps in calls to
// CopyFromBackingStore(). The behavior is controlled by a
// CaptureTestSourceController.
class CaptureTestRenderViewHost : public TestRenderViewHost {
 public:
  CaptureTestRenderViewHost(SiteInstance* instance,
                            RenderViewHostDelegate* delegate,
                            RenderWidgetHostDelegate* widget_delegate,
                            int routing_id,
                            int main_frame_routing_id,
                            bool swapped_out,
                            CaptureTestSourceController* controller)
      : TestRenderViewHost(instance, delegate, widget_delegate, routing_id,
                           main_frame_routing_id, swapped_out),
        controller_(controller) {
    // Override the default view installed by TestRenderViewHost; we need
    // our special subclass which has mocked-out tab capture support.
    RenderWidgetHostView* old_view = GetView();
    SetView(new CaptureTestView(this, controller));
    delete old_view;
  }

  // TestRenderViewHost overrides.
  virtual void CopyFromBackingStore(
      const gfx::Rect& src_rect,
      const gfx::Size& accelerated_dst_size,
      const base::Callback<void(bool, const SkBitmap&)>& callback,
      const SkBitmap::Config& bitmap_config) OVERRIDE {
    gfx::Size size = controller_->GetCopyResultSize();
    SkColor color = controller_->GetSolidColor();

    // Although it's not necessary, use a PlatformBitmap here (instead of a
    // regular SkBitmap) to exercise possible threading issues.
    skia::PlatformBitmap output;
    EXPECT_TRUE(output.Allocate(size.width(), size.height(), false));
    {
      SkAutoLockPixels locker(output.GetBitmap());
      output.GetBitmap().eraseColor(color);
    }
    callback.Run(true, output.GetBitmap());
    controller_->SignalCopy();
  }

 private:
  CaptureTestSourceController* controller_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(CaptureTestRenderViewHost);
};

#if defined(COMPILER_MSVC)
// Re-enable warning 4250
#pragma warning(pop)
#endif

class CaptureTestRenderViewHostFactory : public RenderViewHostFactory {
 public:
  explicit CaptureTestRenderViewHostFactory(
      CaptureTestSourceController* controller) : controller_(controller) {
    RegisterFactory(this);
  }

  virtual ~CaptureTestRenderViewHostFactory() {
    UnregisterFactory();
  }

  // RenderViewHostFactory implementation.
  virtual RenderViewHost* CreateRenderViewHost(
      SiteInstance* instance,
      RenderViewHostDelegate* delegate,
      RenderWidgetHostDelegate* widget_delegate,
      int routing_id,
      int main_frame_routing_id,
      bool swapped_out) OVERRIDE {
    return new CaptureTestRenderViewHost(instance, delegate, widget_delegate,
                                         routing_id, main_frame_routing_id,
                                         swapped_out, controller_);
  }
 private:
  CaptureTestSourceController* controller_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(CaptureTestRenderViewHostFactory);
};

// A stub consumer of captured video frames, which checks the output of
// WebContentsVideoCaptureDevice.
class StubClient : public media::VideoCaptureDevice::Client {
 public:
  StubClient(const base::Callback<void(SkColor)>& color_callback,
             const base::Closure& error_callback)
      : color_callback_(color_callback),
        error_callback_(error_callback) {
    buffer_pool_ = new VideoCaptureBufferPool(2);
  }
  virtual ~StubClient() {}

  virtual scoped_refptr<media::VideoCaptureDevice::Client::Buffer>
  ReserveOutputBuffer(media::VideoFrame::Format format,
                      const gfx::Size& dimensions) OVERRIDE {
    CHECK_EQ(format, media::VideoFrame::I420);
    const size_t frame_bytes =
        media::VideoFrame::AllocationSize(media::VideoFrame::I420, dimensions);
    int buffer_id_to_drop = VideoCaptureBufferPool::kInvalidId;  // Ignored.
    int buffer_id =
        buffer_pool_->ReserveForProducer(frame_bytes, &buffer_id_to_drop);
    if (buffer_id == VideoCaptureBufferPool::kInvalidId)
      return NULL;
    void* data;
    size_t size;
    buffer_pool_->GetBufferInfo(buffer_id, &data, &size);
    return scoped_refptr<media::VideoCaptureDevice::Client::Buffer>(
        new PoolBuffer(buffer_pool_, buffer_id, data, size));
  }

  virtual void OnIncomingCapturedData(
      const uint8* data,
      int length,
      const media::VideoCaptureFormat& frame_format,
      int rotation,
      base::TimeTicks timestamp) OVERRIDE {
    FAIL();
  }

  virtual void OnIncomingCapturedVideoFrame(
      const scoped_refptr<Buffer>& buffer,
      const media::VideoCaptureFormat& buffer_format,
      const scoped_refptr<media::VideoFrame>& frame,
      base::TimeTicks timestamp) OVERRIDE {
    EXPECT_EQ(gfx::Size(kTestWidth, kTestHeight), buffer_format.frame_size);
    EXPECT_EQ(media::PIXEL_FORMAT_I420, buffer_format.pixel_format);
    EXPECT_EQ(media::VideoFrame::I420, frame->format());
    uint8 yuv[3];
    for (int plane = 0; plane < 3; ++plane)
      yuv[plane] = frame->data(plane)[0];
    // TODO(nick): We just look at the first pixel presently, because if
    // the analysis is too slow, the backlog of frames will grow without bound
    // and trouble erupts. http://crbug.com/174519
    color_callback_.Run((SkColorSetRGB(yuv[0], yuv[1], yuv[2])));
  }

  virtual void OnError(const std::string& reason) OVERRIDE {
    error_callback_.Run();
  }

 private:
  class PoolBuffer : public media::VideoCaptureDevice::Client::Buffer {
   public:
    PoolBuffer(const scoped_refptr<VideoCaptureBufferPool>& pool,
               int buffer_id,
               void* data,
               size_t size)
        : Buffer(buffer_id, data, size), pool_(pool) {}

   private:
    virtual ~PoolBuffer() { pool_->RelinquishProducerReservation(id()); }
    const scoped_refptr<VideoCaptureBufferPool> pool_;
  };

  scoped_refptr<VideoCaptureBufferPool> buffer_pool_;
  base::Callback<void(SkColor)> color_callback_;
  base::Closure error_callback_;

  DISALLOW_COPY_AND_ASSIGN(StubClient);
};

class StubClientObserver {
 public:
  StubClientObserver()
      : error_encountered_(false),
        wait_color_yuv_(0xcafe1950) {
    client_.reset(new StubClient(
        base::Bind(&StubClientObserver::OnColor, base::Unretained(this)),
        base::Bind(&StubClientObserver::OnError, base::Unretained(this))));
  }

  virtual ~StubClientObserver() {}

  scoped_ptr<media::VideoCaptureDevice::Client> PassClient() {
    return client_.PassAs<media::VideoCaptureDevice::Client>();
  }

  void QuitIfConditionMet(SkColor color) {
    base::AutoLock guard(lock_);
    if (wait_color_yuv_ == color || error_encountered_)
      base::MessageLoop::current()->Quit();
  }

  void WaitForNextColor(SkColor expected_color) {
    {
      base::AutoLock guard(lock_);
      wait_color_yuv_ = ConvertRgbToYuv(expected_color);
      error_encountered_ = false;
    }
    RunCurrentLoopWithDeadline();
    {
      base::AutoLock guard(lock_);
      ASSERT_FALSE(error_encountered_);
    }
  }

  void WaitForError() {
    {
      base::AutoLock guard(lock_);
      wait_color_yuv_ = kNotInterested;
      error_encountered_ = false;
    }
    RunCurrentLoopWithDeadline();
    {
      base::AutoLock guard(lock_);
      ASSERT_TRUE(error_encountered_);
    }
  }

  bool HasError() {
    base::AutoLock guard(lock_);
    return error_encountered_;
  }

  void OnError() {
    {
      base::AutoLock guard(lock_);
      error_encountered_ = true;
    }
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
        &StubClientObserver::QuitIfConditionMet,
        base::Unretained(this),
        kNothingYet));
  }

  void OnColor(SkColor color) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
        &StubClientObserver::QuitIfConditionMet,
        base::Unretained(this),
        color));
  }

 private:
  base::Lock lock_;
  bool error_encountered_;
  SkColor wait_color_yuv_;
  scoped_ptr<StubClient> client_;

  DISALLOW_COPY_AND_ASSIGN(StubClientObserver);
};

// Test harness that sets up a minimal environment with necessary stubs.
class WebContentsVideoCaptureDeviceTest : public testing::Test {
 public:
  // This is public because C++ method pointer scoping rules are silly and make
  // this hard to use with Bind().
  void ResetWebContents() {
    web_contents_.reset();
  }

 protected:
  virtual void SetUp() {
    // TODO(nick): Sadness and woe! Much "mock-the-world" boilerplate could be
    // eliminated here, if only we could use RenderViewHostTestHarness. The
    // catch is that we need our TestRenderViewHost to support a
    // CopyFromBackingStore operation that we control. To accomplish that,
    // either RenderViewHostTestHarness would have to support installing a
    // custom RenderViewHostFactory, or else we implant some kind of delegated
    // CopyFromBackingStore functionality into TestRenderViewHost itself.

    render_process_host_factory_.reset(new MockRenderProcessHostFactory());
    // Create our (self-registering) RVH factory, so that when we create a
    // WebContents, it in turn creates CaptureTestRenderViewHosts.
    render_view_host_factory_.reset(
        new CaptureTestRenderViewHostFactory(&controller_));

    browser_context_.reset(new TestBrowserContext());

    scoped_refptr<SiteInstance> site_instance =
        SiteInstance::Create(browser_context_.get());
    SiteInstanceImpl::set_render_process_host_factory(
        render_process_host_factory_.get());
    web_contents_.reset(
        TestWebContents::Create(browser_context_.get(), site_instance.get()));

    // This is actually a CaptureTestRenderViewHost.
    RenderWidgetHostImpl* rwh =
        RenderWidgetHostImpl::From(web_contents_->GetRenderViewHost());

    std::string device_id =
        WebContentsCaptureUtil::AppendWebContentsDeviceScheme(
            base::StringPrintf("%d:%d", rwh->GetProcess()->GetID(),
                               rwh->GetRoutingID()));

    device_.reset(WebContentsVideoCaptureDevice::Create(device_id));

    base::RunLoop().RunUntilIdle();
  }

  virtual void TearDown() {
    // Tear down in opposite order of set-up.

    // The device is destroyed asynchronously, and will notify the
    // CaptureTestSourceController when it finishes destruction.
    // Trigger this, and wait.
    if (device_) {
      device_->StopAndDeAllocate();
      device_.reset();
    }

    base::RunLoop().RunUntilIdle();

    // Destroy the browser objects.
    web_contents_.reset();
    browser_context_.reset();

    base::RunLoop().RunUntilIdle();

    SiteInstanceImpl::set_render_process_host_factory(NULL);
    render_view_host_factory_.reset();
    render_process_host_factory_.reset();
  }

  // Accessors.
  CaptureTestSourceController* source() { return &controller_; }
  media::VideoCaptureDevice* device() { return device_.get(); }

  void SimulateDrawEvent() {
    if (source()->CanUseFrameSubscriber()) {
      // Print
      CaptureTestView* test_view = static_cast<CaptureTestView*>(
          web_contents_->GetRenderViewHost()->GetView());
      test_view->SimulateUpdate();
    } else {
      // Simulate a non-accelerated paint.
      NotificationService::current()->Notify(
          NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
          Source<RenderWidgetHost>(web_contents_->GetRenderViewHost()),
          NotificationService::NoDetails());
    }
  }

  void DestroyVideoCaptureDevice() { device_.reset(); }

  StubClientObserver* client_observer() {
    return &client_observer_;
  }

 private:
  StubClientObserver client_observer_;

  // The controller controls which pixel patterns to produce.
  CaptureTestSourceController controller_;

  // Self-registering RenderProcessHostFactory.
  scoped_ptr<MockRenderProcessHostFactory> render_process_host_factory_;

  // Creates capture-capable RenderViewHosts whose pixel content production is
  // under the control of |controller_|.
  scoped_ptr<CaptureTestRenderViewHostFactory> render_view_host_factory_;

  // A mocked-out browser and tab.
  scoped_ptr<TestBrowserContext> browser_context_;
  scoped_ptr<WebContents> web_contents_;

  // Finally, the WebContentsVideoCaptureDevice under test.
  scoped_ptr<media::VideoCaptureDevice> device_;

  TestBrowserThreadBundle thread_bundle_;
};

TEST_F(WebContentsVideoCaptureDeviceTest, InvalidInitialWebContentsError) {
  // Before the installs itself on the UI thread up to start capturing, we'll
  // delete the web contents. This should trigger an error which can happen in
  // practice; we should be able to recover gracefully.
  ResetWebContents();

  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestWidth, kTestHeight);
  capture_params.requested_format.frame_rate = kTestFramesPerSecond;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  capture_params.allow_resolution_change = false;
  device()->AllocateAndStart(capture_params, client_observer()->PassClient());
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForError());
  device()->StopAndDeAllocate();
}

TEST_F(WebContentsVideoCaptureDeviceTest, WebContentsDestroyed) {
  // We'll simulate the tab being closed after the capture pipeline is up and
  // running.
  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestWidth, kTestHeight);
  capture_params.requested_format.frame_rate = kTestFramesPerSecond;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  capture_params.allow_resolution_change = false;
  device()->AllocateAndStart(capture_params, client_observer()->PassClient());
  // Do one capture to prove
  source()->SetSolidColor(SK_ColorRED);
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorRED));

  base::RunLoop().RunUntilIdle();

  // Post a task to close the tab. We should see an error reported to the
  // consumer.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&WebContentsVideoCaptureDeviceTest::ResetWebContents,
                 base::Unretained(this)));
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForError());
  device()->StopAndDeAllocate();
}

TEST_F(WebContentsVideoCaptureDeviceTest,
       StopDeviceBeforeCaptureMachineCreation) {
  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestWidth, kTestHeight);
  capture_params.requested_format.frame_rate = kTestFramesPerSecond;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  capture_params.allow_resolution_change = false;
  device()->AllocateAndStart(capture_params, client_observer()->PassClient());

  // Make a point of not running the UI messageloop here.
  device()->StopAndDeAllocate();
  DestroyVideoCaptureDevice();

  // Currently, there should be CreateCaptureMachineOnUIThread() and
  // DestroyCaptureMachineOnUIThread() tasks pending on the current (UI) message
  // loop. These should both succeed without crashing, and the machine should
  // wind up in the idle state.
  base::RunLoop().RunUntilIdle();
}

TEST_F(WebContentsVideoCaptureDeviceTest, StopWithRendererWorkToDo) {
  // Set up the test to use RGB copies and an normal
  source()->SetCanCopyToVideoFrame(false);
  source()->SetUseFrameSubscriber(false);
  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestWidth, kTestHeight);
  capture_params.requested_format.frame_rate = kTestFramesPerSecond;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  capture_params.allow_resolution_change = false;
  device()->AllocateAndStart(capture_params, client_observer()->PassClient());

  base::RunLoop().RunUntilIdle();

  for (int i = 0; i < 10; ++i)
    SimulateDrawEvent();

  ASSERT_FALSE(client_observer()->HasError());
  device()->StopAndDeAllocate();
  ASSERT_FALSE(client_observer()->HasError());
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(client_observer()->HasError());
}

TEST_F(WebContentsVideoCaptureDeviceTest, DeviceRestart) {
  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestWidth, kTestHeight);
  capture_params.requested_format.frame_rate = kTestFramesPerSecond;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  capture_params.allow_resolution_change = false;
  device()->AllocateAndStart(capture_params, client_observer()->PassClient());
  base::RunLoop().RunUntilIdle();
  source()->SetSolidColor(SK_ColorRED);
  SimulateDrawEvent();
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorRED));
  SimulateDrawEvent();
  SimulateDrawEvent();
  source()->SetSolidColor(SK_ColorGREEN);
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorGREEN));
  device()->StopAndDeAllocate();

  // Device is stopped, but content can still be animating.
  SimulateDrawEvent();
  SimulateDrawEvent();
  base::RunLoop().RunUntilIdle();

  StubClientObserver observer2;
  device()->AllocateAndStart(capture_params, observer2.PassClient());
  source()->SetSolidColor(SK_ColorBLUE);
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(observer2.WaitForNextColor(SK_ColorBLUE));
  source()->SetSolidColor(SK_ColorYELLOW);
  SimulateDrawEvent();
  ASSERT_NO_FATAL_FAILURE(observer2.WaitForNextColor(SK_ColorYELLOW));
  device()->StopAndDeAllocate();
}

// The "happy case" test.  No scaling is needed, so we should be able to change
// the picture emitted from the source and expect to see each delivered to the
// consumer. The test will alternate between the three capture paths, simulating
// falling in and out of accelerated compositing.
TEST_F(WebContentsVideoCaptureDeviceTest, GoesThroughAllTheMotions) {
  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestWidth, kTestHeight);
  capture_params.requested_format.frame_rate = kTestFramesPerSecond;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  capture_params.allow_resolution_change = false;
  device()->AllocateAndStart(capture_params, client_observer()->PassClient());

  for (int i = 0; i < 6; i++) {
    const char* name = NULL;
    switch (i % 3) {
      case 0:
        source()->SetCanCopyToVideoFrame(true);
        source()->SetUseFrameSubscriber(false);
        name = "VideoFrame";
        break;
      case 1:
        source()->SetCanCopyToVideoFrame(false);
        source()->SetUseFrameSubscriber(true);
        name = "Subscriber";
        break;
      case 2:
        source()->SetCanCopyToVideoFrame(false);
        source()->SetUseFrameSubscriber(false);
        name = "SkBitmap";
        break;
      default:
        FAIL();
    }

    SCOPED_TRACE(base::StringPrintf("Using %s path, iteration #%d", name, i));

    source()->SetSolidColor(SK_ColorRED);
    SimulateDrawEvent();
    ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorRED));

    source()->SetSolidColor(SK_ColorGREEN);
    SimulateDrawEvent();
    ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorGREEN));

    source()->SetSolidColor(SK_ColorBLUE);
    SimulateDrawEvent();
    ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorBLUE));

    source()->SetSolidColor(SK_ColorBLACK);
    SimulateDrawEvent();
    ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorBLACK));
  }
  device()->StopAndDeAllocate();
}

TEST_F(WebContentsVideoCaptureDeviceTest, RejectsInvalidAllocateParams) {
  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(1280, 720);
  capture_params.requested_format.frame_rate = -2;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  capture_params.allow_resolution_change = false;
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&media::VideoCaptureDevice::AllocateAndStart,
                 base::Unretained(device()),
                 capture_params,
                 base::Passed(client_observer()->PassClient())));
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForError());
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&media::VideoCaptureDevice::StopAndDeAllocate,
                 base::Unretained(device())));
  base::RunLoop().RunUntilIdle();
}

TEST_F(WebContentsVideoCaptureDeviceTest, BadFramesGoodFrames) {
  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestWidth, kTestHeight);
  capture_params.requested_format.frame_rate = kTestFramesPerSecond;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  capture_params.allow_resolution_change = false;
  // 1x1 is too small to process; we intend for this to result in an error.
  source()->SetCopyResultSize(1, 1);
  source()->SetSolidColor(SK_ColorRED);
  device()->AllocateAndStart(capture_params, client_observer()->PassClient());

  // These frames ought to be dropped during the Render stage. Let
  // several captures to happen.
  ASSERT_NO_FATAL_FAILURE(source()->WaitForNextCopy());
  ASSERT_NO_FATAL_FAILURE(source()->WaitForNextCopy());
  ASSERT_NO_FATAL_FAILURE(source()->WaitForNextCopy());
  ASSERT_NO_FATAL_FAILURE(source()->WaitForNextCopy());
  ASSERT_NO_FATAL_FAILURE(source()->WaitForNextCopy());

  // Now push some good frames through; they should be processed normally.
  source()->SetCopyResultSize(kTestWidth, kTestHeight);
  source()->SetSolidColor(SK_ColorGREEN);
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorGREEN));
  source()->SetSolidColor(SK_ColorRED);
  ASSERT_NO_FATAL_FAILURE(client_observer()->WaitForNextColor(SK_ColorRED));

  device()->StopAndDeAllocate();
}

}  // namespace
}  // namespace content
