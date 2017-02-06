// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_aura.h"

#include <stddef.h>
#include <tuple>

#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigation_entry_screenshot_manager.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_switches.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace {

// TODO(tdresser): Find a way to avoid sleeping like this. See crbug.com/405282
// for details.
void GiveItSomeTime() {
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMillisecondsD(10));
  run_loop.Run();
}

// WebContentsDelegate which tracks vertical overscroll updates.
class VerticalOverscrollTracker : public content::WebContentsDelegate {
 public:
  VerticalOverscrollTracker() : count_(0), completed_(false) {}
  ~VerticalOverscrollTracker() override {}

  int num_overscroll_updates() const {
    return count_;
  }

  bool overscroll_completed() const {
    return completed_;
  }

  void Reset() {
    count_ = 0;
    completed_ = false;
  }

 private:
  bool CanOverscrollContent() const override { return true; }

  void OverscrollUpdate(float delta_y) override { ++count_; }

  void OverscrollComplete() override { completed_ = true; }

  int count_;
  bool completed_;

  DISALLOW_COPY_AND_ASSIGN(VerticalOverscrollTracker);
};

}  //namespace


namespace content {

// This class keeps track of the RenderViewHost whose screenshot was captured.
class ScreenshotTracker : public NavigationEntryScreenshotManager {
 public:
  explicit ScreenshotTracker(NavigationControllerImpl* controller)
      : NavigationEntryScreenshotManager(controller),
        screenshot_taken_for_(NULL),
        waiting_for_screenshots_(0) {
  }

  ~ScreenshotTracker() override {}

  RenderViewHost* screenshot_taken_for() { return screenshot_taken_for_; }

  void Reset() {
    screenshot_taken_for_ = NULL;
    screenshot_set_.clear();
  }

  void SetScreenshotInterval(int interval_ms) {
    SetMinScreenshotIntervalMS(interval_ms);
  }

  void WaitUntilScreenshotIsReady() {
    if (!waiting_for_screenshots_)
      return;
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
  }

  bool ScreenshotSetForEntry(NavigationEntryImpl* entry) const {
    return screenshot_set_.count(entry) > 0;
  }

 private:
  // Overridden from NavigationEntryScreenshotManager:
  void TakeScreenshotImpl(RenderViewHost* host,
                          NavigationEntryImpl* entry) override {
    ++waiting_for_screenshots_;
    screenshot_taken_for_ = host;
    NavigationEntryScreenshotManager::TakeScreenshotImpl(host, entry);
  }

  void OnScreenshotSet(NavigationEntryImpl* entry) override {
    --waiting_for_screenshots_;
    screenshot_set_[entry] = true;
    NavigationEntryScreenshotManager::OnScreenshotSet(entry);
    if (waiting_for_screenshots_ == 0 && message_loop_runner_.get())
      message_loop_runner_->Quit();
  }

  RenderViewHost* screenshot_taken_for_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  int waiting_for_screenshots_;
  std::map<NavigationEntryImpl*, bool> screenshot_set_;

  DISALLOW_COPY_AND_ASSIGN(ScreenshotTracker);
};

class NavigationWatcher : public WebContentsObserver {
 public:
  explicit NavigationWatcher(WebContents* contents)
      : WebContentsObserver(contents),
        navigated_(false),
        should_quit_loop_(false) {
  }

  ~NavigationWatcher() override {}

  void WaitUntilNavigationStarts() {
    if (navigated_)
      return;
    should_quit_loop_ = true;
    base::MessageLoop::current()->Run();
  }

 private:
  // Overridden from WebContentsObserver:
  void DidStartNavigationToPendingEntry(
      const GURL& validated_url,
      NavigationController::ReloadType reload_type) override {
    navigated_ = true;
    if (should_quit_loop_)
      base::MessageLoop::current()->QuitWhenIdle();
  }

  bool navigated_;
  bool should_quit_loop_;

  DISALLOW_COPY_AND_ASSIGN(NavigationWatcher);
};

class InputEventMessageFilterWaitsForAcks : public BrowserMessageFilter {
 public:
  InputEventMessageFilterWaitsForAcks()
      : BrowserMessageFilter(InputMsgStart),
        type_(blink::WebInputEvent::Undefined),
        state_(INPUT_EVENT_ACK_STATE_UNKNOWN) {}

  void WaitForAck(blink::WebInputEvent::Type type) {
    base::RunLoop run_loop;
    base::AutoReset<base::Closure> reset_quit(&quit_, run_loop.QuitClosure());
    base::AutoReset<blink::WebInputEvent::Type> reset_type(&type_, type);
    run_loop.Run();
  }

  InputEventAckState last_ack_state() const { return state_; }

 protected:
  ~InputEventMessageFilterWaitsForAcks() override {}

 private:
  void ReceivedEventAck(blink::WebInputEvent::Type type,
                        InputEventAckState state) {
    if (type_ == type) {
      state_ = state;
      quit_.Run();
    }
  }

  // BrowserMessageFilter:
  bool OnMessageReceived(const IPC::Message& message) override {
    if (message.type() == InputHostMsg_HandleInputEvent_ACK::ID) {
      InputHostMsg_HandleInputEvent_ACK::Param params;
      InputHostMsg_HandleInputEvent_ACK::Read(&message, &params);
      blink::WebInputEvent::Type type = std::get<0>(params).type;
      InputEventAckState ack = std::get<0>(params).state;
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          base::Bind(&InputEventMessageFilterWaitsForAcks::ReceivedEventAck,
                     this, type, ack));
    }
    return false;
  }

  base::Closure quit_;
  blink::WebInputEvent::Type type_;
  InputEventAckState state_;

  DISALLOW_COPY_AND_ASSIGN(InputEventMessageFilterWaitsForAcks);
};

class WebContentsViewAuraTest : public ContentBrowserTest {
 public:
  WebContentsViewAuraTest()
      : screenshot_manager_(NULL) {
  }

  // Executes the javascript synchronously and makes sure the returned value is
  // freed properly.
  void ExecuteSyncJSFunction(RenderFrameHost* rfh, const std::string& jscript) {
    std::unique_ptr<base::Value> value =
        content::ExecuteScriptAndGetValue(rfh, jscript);
  }

  // Starts the test server and navigates to the given url. Sets a large enough
  // size to the root window.  Returns after the navigation to the url is
  // complete.
  void StartTestWithPage(const std::string& url) {
    ASSERT_TRUE(embedded_test_server()->Start());
    GURL test_url;
    if (url == "about:blank")
      test_url = GURL(url);
    else
      test_url = GURL(embedded_test_server()->GetURL(url));
    NavigateToURL(shell(), test_url);

    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    NavigationControllerImpl* controller = &web_contents->GetController();

    screenshot_manager_ = new ScreenshotTracker(controller);
    controller->SetScreenshotManager(base::WrapUnique(screenshot_manager_));

    frame_watcher_ = new FrameWatcher();
    frame_watcher_->AttachTo(shell()->web_contents());
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitchASCII(switches::kTouchEvents,
                           switches::kTouchEventsEnabled);
  }

  void TestOverscrollNavigation(bool touch_handler) {
    ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/overscroll_navigation.html"));
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    NavigationController& controller = web_contents->GetController();
    RenderFrameHost* main_frame = web_contents->GetMainFrame();

    EXPECT_FALSE(controller.CanGoBack());
    EXPECT_FALSE(controller.CanGoForward());
    int index = -1;
    std::unique_ptr<base::Value> value =
        content::ExecuteScriptAndGetValue(main_frame, "get_current()");
    ASSERT_TRUE(value->GetAsInteger(&index));
    EXPECT_EQ(0, index);

    if (touch_handler)
      ExecuteSyncJSFunction(main_frame, "install_touch_handler()");

    ExecuteSyncJSFunction(main_frame, "navigate_next()");
    ExecuteSyncJSFunction(main_frame, "navigate_next()");
    value = content::ExecuteScriptAndGetValue(main_frame, "get_current()");
    ASSERT_TRUE(value->GetAsInteger(&index));
    EXPECT_EQ(2, index);
    EXPECT_TRUE(controller.CanGoBack());
    EXPECT_FALSE(controller.CanGoForward());

    aura::Window* content = web_contents->GetContentNativeView();
    gfx::Rect bounds = content->GetBoundsInRootWindow();
    ui::test::EventGenerator generator(content->GetRootWindow(), content);
    const int kScrollDurationMs = 20;
    const int kScrollSteps = 10;

    {
      // Do a swipe-right now. That should navigate backwards.
      base::string16 expected_title = base::ASCIIToUTF16("Title: #1");
      content::TitleWatcher title_watcher(web_contents, expected_title);
      generator.GestureScrollSequence(
          gfx::Point(bounds.x() + 2, bounds.y() + 10),
          gfx::Point(bounds.right() - 10, bounds.y() + 10),
          base::TimeDelta::FromMilliseconds(kScrollDurationMs),
          kScrollSteps);
      base::string16 actual_title = title_watcher.WaitAndGetTitle();
      EXPECT_EQ(expected_title, actual_title);
      value = content::ExecuteScriptAndGetValue(main_frame, "get_current()");
      ASSERT_TRUE(value->GetAsInteger(&index));
      EXPECT_EQ(1, index);
      EXPECT_TRUE(controller.CanGoBack());
      EXPECT_TRUE(controller.CanGoForward());
    }

    {
      // Do a fling-right now. That should navigate backwards.
      base::string16 expected_title = base::ASCIIToUTF16("Title:");
      content::TitleWatcher title_watcher(web_contents, expected_title);
      generator.GestureScrollSequence(
          gfx::Point(bounds.x() + 2, bounds.y() + 10),
          gfx::Point(bounds.right() - 10, bounds.y() + 10),
          base::TimeDelta::FromMilliseconds(kScrollDurationMs),
          kScrollSteps);
      base::string16 actual_title = title_watcher.WaitAndGetTitle();
      EXPECT_EQ(expected_title, actual_title);
      value = content::ExecuteScriptAndGetValue(main_frame, "get_current()");
      ASSERT_TRUE(value->GetAsInteger(&index));
      EXPECT_EQ(0, index);
      EXPECT_FALSE(controller.CanGoBack());
      EXPECT_TRUE(controller.CanGoForward());
    }

    {
      // Do a swipe-left now. That should navigate forward.
      base::string16 expected_title = base::ASCIIToUTF16("Title: #1");
      content::TitleWatcher title_watcher(web_contents, expected_title);
      generator.GestureScrollSequence(
          gfx::Point(bounds.right() - 10, bounds.y() + 10),
          gfx::Point(bounds.x() + 2, bounds.y() + 10),
          base::TimeDelta::FromMilliseconds(kScrollDurationMs),
          kScrollSteps);
      base::string16 actual_title = title_watcher.WaitAndGetTitle();
      EXPECT_EQ(expected_title, actual_title);
      value = content::ExecuteScriptAndGetValue(main_frame, "get_current()");
      ASSERT_TRUE(value->GetAsInteger(&index));
      EXPECT_EQ(1, index);
      EXPECT_TRUE(controller.CanGoBack());
      EXPECT_TRUE(controller.CanGoForward());
    }
  }

  int GetCurrentIndex() {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    RenderFrameHost* main_frame = web_contents->GetMainFrame();
    int index = -1;
    std::unique_ptr<base::Value> value;
    value = content::ExecuteScriptAndGetValue(main_frame, "get_current()");
    if (!value->GetAsInteger(&index))
      index = -1;
    return index;
  }

  int ExecuteScriptAndExtractInt(const std::string& script) {
    int value = 0;
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        shell(), "domAutomationController.send(" + script + ")", &value));
    return value;
  }

  RenderViewHost* GetRenderViewHost() const {
    RenderViewHost* const rvh = shell()->web_contents()->GetRenderViewHost();
    CHECK(rvh);
    return rvh;
  }

  RenderWidgetHostImpl* GetRenderWidgetHost() const {
    RenderWidgetHostImpl* const rwh =
        RenderWidgetHostImpl::From(shell()
                                       ->web_contents()
                                       ->GetRenderWidgetHostView()
                                       ->GetRenderWidgetHost());
    CHECK(rwh);
    return rwh;
  }

  RenderWidgetHostViewBase* GetRenderWidgetHostView() const {
    return static_cast<RenderWidgetHostViewBase*>(
        GetRenderViewHost()->GetWidget()->GetView());
  }

  InputEventMessageFilterWaitsForAcks* filter() {
    return filter_.get();
  }

  void WaitAFrame() {
    while (!GetRenderWidgetHost()->ScheduleComposite())
      GiveItSomeTime();
    frame_watcher_->WaitFrames(1);
  }

 protected:
  ScreenshotTracker* screenshot_manager() { return screenshot_manager_; }
  void set_min_screenshot_interval(int interval_ms) {
    screenshot_manager_->SetScreenshotInterval(interval_ms);
  }

  void AddInputEventMessageFilter() {
    filter_ = new InputEventMessageFilterWaitsForAcks();
    GetRenderWidgetHost()->GetProcess()->AddFilter(filter_.get());
  }

 private:
  ScreenshotTracker* screenshot_manager_;
  scoped_refptr<InputEventMessageFilterWaitsForAcks> filter_;
  scoped_refptr<FrameWatcher> frame_watcher_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsViewAuraTest);
};

// Flaky on Windows: http://crbug.com/305722
// The test frequently times out on Linux, too. See crbug.com/440043.
#if defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_OverscrollNavigation DISABLED_OverscrollNavigation
#else
#define MAYBE_OverscrollNavigation OverscrollNavigation
#endif

IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest, MAYBE_OverscrollNavigation) {
  TestOverscrollNavigation(false);
}

// Flaky on Windows (might be related to the above test):
// http://crbug.com/305722
// On Linux, the test frequently times out. (See crbug.com/440043).
#if defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_OverscrollNavigationWithTouchHandler \
        DISABLED_OverscrollNavigationWithTouchHandler
#else
#define MAYBE_OverscrollNavigationWithTouchHandler \
        OverscrollNavigationWithTouchHandler
#endif
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest,
                       MAYBE_OverscrollNavigationWithTouchHandler) {
  TestOverscrollNavigation(true);
}

// Disabled because the test always fails the first time it runs on the Win Aura
// bots, and usually but not always passes second-try (See crbug.com/179532).
// On Linux, the test frequently times out. (See crbug.com/440043).
#if defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_QuickOverscrollDirectionChange \
        DISABLED_QuickOverscrollDirectionChange
#else
#define MAYBE_QuickOverscrollDirectionChange QuickOverscrollDirectionChange
#endif
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest,
                       MAYBE_QuickOverscrollDirectionChange) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/overscroll_navigation.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHost* main_frame = web_contents->GetMainFrame();

  // This test triggers a large number of animations. Speed them up to ensure
  // the test completes within its time limit.
  ui::ScopedAnimationDurationScaleMode fast_duration_mode(
      ui::ScopedAnimationDurationScaleMode::FAST_DURATION);

  // Make sure the page has both back/forward history.
  ExecuteSyncJSFunction(main_frame, "navigate_next()");
  EXPECT_EQ(1, GetCurrentIndex());
  ExecuteSyncJSFunction(main_frame, "navigate_next()");
  EXPECT_EQ(2, GetCurrentIndex());
  web_contents->GetController().GoBack();
  EXPECT_EQ(1, GetCurrentIndex());

  aura::Window* content = web_contents->GetContentNativeView();
  ui::EventProcessor* dispatcher = content->GetHost()->event_processor();
  gfx::Rect bounds = content->GetBoundsInRootWindow();

  base::TimeTicks timestamp = ui::EventTimeForNow();
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED,
      gfx::Point(bounds.x() + bounds.width() / 2, bounds.y() + 5), 0,
      timestamp);
  ui::EventDispatchDetails details = dispatcher->OnEventFromSource(&press);
  ASSERT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(1, GetCurrentIndex());

  timestamp += base::TimeDelta::FromMilliseconds(10);
  ui::TouchEvent move1(ui::ET_TOUCH_MOVED,
                       gfx::Point(bounds.right() - 10, bounds.y() + 5), 0,
                       timestamp);
  details = dispatcher->OnEventFromSource(&move1);
  ASSERT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(1, GetCurrentIndex());

  // Swipe back from the right edge, back to the left edge, back to the right
  // edge.

  for (int x = bounds.right() - 10; x >= bounds.x() + 10; x-= 10) {
    timestamp += base::TimeDelta::FromMilliseconds(10);
    ui::TouchEvent inc(ui::ET_TOUCH_MOVED, gfx::Point(x, bounds.y() + 5), 0,
                       timestamp);
    details = dispatcher->OnEventFromSource(&inc);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(1, GetCurrentIndex());
  }

  for (int x = bounds.x() + 10; x <= bounds.width() - 10; x+= 10) {
    timestamp += base::TimeDelta::FromMilliseconds(10);
    ui::TouchEvent inc(ui::ET_TOUCH_MOVED, gfx::Point(x, bounds.y() + 5), 0,
                       timestamp);
    details = dispatcher->OnEventFromSource(&inc);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(1, GetCurrentIndex());
  }

  for (int x = bounds.width() - 10; x >= bounds.x() + 10; x-= 10) {
    timestamp += base::TimeDelta::FromMilliseconds(10);
    ui::TouchEvent inc(ui::ET_TOUCH_MOVED, gfx::Point(x, bounds.y() + 5), 0,
                       timestamp);
    details = dispatcher->OnEventFromSource(&inc);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(1, GetCurrentIndex());
  }

  // Do not end the overscroll sequence.
}

// Tests that the page has has a screenshot when navigation happens:
//  - from within the page (from a JS function)
//  - interactively, when user does an overscroll gesture
//  - interactively, when user navigates in history without the overscroll
//    gesture.
// Flaky on Windows (http://crbug.com/357311). Might be related to
// OverscrollNavigation test.
// Flaky on Ozone (http://crbug.com/399676).
// Flaky on ChromeOS (http://crbug.com/405945).
#if defined(OS_WIN) || defined(USE_OZONE) || defined(OS_CHROMEOS)
#define MAYBE_OverscrollScreenshot DISABLED_OverscrollScreenshot
#else
#define MAYBE_OverscrollScreenshot OverscrollScreenshot
#endif
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest, MAYBE_OverscrollScreenshot) {
  // Disable the test for WinXP.  See http://crbug/294116.
#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    LOG(WARNING) << "Test disabled due to unknown bug on WinXP.";
    return;
  }
#endif

  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/overscroll_navigation.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHost* main_frame = web_contents->GetMainFrame();

  set_min_screenshot_interval(0);

  // Do a few navigations initiated by the page.
  // Screenshots should never be captured since these are all in-page
  // navigations.
  ExecuteSyncJSFunction(main_frame, "navigate_next()");
  EXPECT_EQ(1, GetCurrentIndex());
  ExecuteSyncJSFunction(main_frame, "navigate_next()");
  EXPECT_EQ(2, GetCurrentIndex());
  screenshot_manager()->WaitUntilScreenshotIsReady();

  NavigationEntryImpl* entry = web_contents->GetController().GetEntryAtIndex(2);
  EXPECT_FALSE(entry->screenshot().get());

  entry = web_contents->GetController().GetEntryAtIndex(1);
  EXPECT_FALSE(screenshot_manager()->ScreenshotSetForEntry(entry));

  entry = web_contents->GetController().GetEntryAtIndex(0);
  EXPECT_FALSE(screenshot_manager()->ScreenshotSetForEntry(entry));

  ExecuteSyncJSFunction(main_frame, "navigate_next()");
  screenshot_manager()->WaitUntilScreenshotIsReady();

  entry = web_contents->GetController().GetEntryAtIndex(2);
  EXPECT_FALSE(screenshot_manager()->ScreenshotSetForEntry(entry));

  entry = web_contents->GetController().GetEntryAtIndex(3);
  EXPECT_FALSE(entry->screenshot().get());
  {
    // Now, swipe right to navigate backwards. This should navigate away from
    // index 3 to index 2.
    base::string16 expected_title = base::ASCIIToUTF16("Title: #2");
    content::TitleWatcher title_watcher(web_contents, expected_title);
    aura::Window* content = web_contents->GetContentNativeView();
    gfx::Rect bounds = content->GetBoundsInRootWindow();
    ui::test::EventGenerator generator(content->GetRootWindow(), content);
    generator.GestureScrollSequence(
        gfx::Point(bounds.x() + 2, bounds.y() + 10),
        gfx::Point(bounds.right() - 10, bounds.y() + 10),
        base::TimeDelta::FromMilliseconds(20),
        1);
    base::string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
    EXPECT_EQ(2, GetCurrentIndex());
    screenshot_manager()->WaitUntilScreenshotIsReady();
    entry = web_contents->GetController().GetEntryAtIndex(3);
    EXPECT_FALSE(screenshot_manager()->ScreenshotSetForEntry(entry));
  }

  // Navigate a couple more times.
  ExecuteSyncJSFunction(main_frame, "navigate_next()");
  EXPECT_EQ(3, GetCurrentIndex());
  ExecuteSyncJSFunction(main_frame, "navigate_next()");
  EXPECT_EQ(4, GetCurrentIndex());
  screenshot_manager()->WaitUntilScreenshotIsReady();
  entry = web_contents->GetController().GetEntryAtIndex(4);
  EXPECT_FALSE(entry->screenshot().get());

  {
    // Navigate back in history.
    base::string16 expected_title = base::ASCIIToUTF16("Title: #3");
    content::TitleWatcher title_watcher(web_contents, expected_title);
    web_contents->GetController().GoBack();
    base::string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
    EXPECT_EQ(3, GetCurrentIndex());
    screenshot_manager()->WaitUntilScreenshotIsReady();
    entry = web_contents->GetController().GetEntryAtIndex(4);
    EXPECT_FALSE(screenshot_manager()->ScreenshotSetForEntry(entry));
  }
}

// Crashes under ThreadSanitizer, http://crbug.com/356758.
#if defined(THREAD_SANITIZER)
#define MAYBE_ScreenshotForSwappedOutRenderViews \
    DISABLED_ScreenshotForSwappedOutRenderViews
#else
#define MAYBE_ScreenshotForSwappedOutRenderViews \
    ScreenshotForSwappedOutRenderViews
#endif
// Tests that screenshot is taken correctly when navigation causes a
// RenderViewHost to be swapped out.
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest,
                       MAYBE_ScreenshotForSwappedOutRenderViews) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/overscroll_navigation.html"));
  // Create a new server with a different site.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(https_server.Start());

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  set_min_screenshot_interval(0);

  struct {
    GURL url;
    int transition;
  } navigations[] = {
      {https_server.GetURL("/title1.html"),
       ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR},
      {embedded_test_server()->GetURL("/title2.html"),
       ui::PAGE_TRANSITION_AUTO_BOOKMARK},
      {https_server.GetURL("/title3.html"),
       ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR},
      {GURL(), 0}};

  screenshot_manager()->Reset();
  for (int i = 0; !navigations[i].url.is_empty(); ++i) {
    // Navigate via the user initiating a navigation from the UI.
    NavigationController::LoadURLParams params(navigations[i].url);
    params.transition_type =
        ui::PageTransitionFromInt(navigations[i].transition);

    RenderViewHost* old_host = web_contents->GetRenderViewHost();
    web_contents->GetController().LoadURLWithParams(params);
    WaitForLoadStop(web_contents);
    screenshot_manager()->WaitUntilScreenshotIsReady();

    EXPECT_NE(old_host, web_contents->GetRenderViewHost())
        << navigations[i].url.spec();
    EXPECT_EQ(old_host, screenshot_manager()->screenshot_taken_for());

    NavigationEntryImpl* entry =
        web_contents->GetController().GetEntryAtOffset(-1);
    EXPECT_TRUE(screenshot_manager()->ScreenshotSetForEntry(entry));

    entry = web_contents->GetController().GetLastCommittedEntry();
    EXPECT_FALSE(screenshot_manager()->ScreenshotSetForEntry(entry));
    EXPECT_FALSE(entry->screenshot().get());
    screenshot_manager()->Reset();
  }

  // Increase the minimum interval between taking screenshots.
  set_min_screenshot_interval(60000);

  // Navigate again. This should not take any screenshot because of the
  // increased screenshot interval.
  NavigationController::LoadURLParams params(navigations[0].url);
  params.transition_type = ui::PageTransitionFromInt(navigations[0].transition);
  web_contents->GetController().LoadURLWithParams(params);
  WaitForLoadStop(web_contents);
  screenshot_manager()->WaitUntilScreenshotIsReady();

  EXPECT_EQ(NULL, screenshot_manager()->screenshot_taken_for());
}

// Tests that navigations resulting from reloads, history.replaceState,
// and history.pushState do not capture screenshots.
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest, ReplaceStateReloadPushState) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/overscroll_navigation.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHost* main_frame = web_contents->GetMainFrame();

  set_min_screenshot_interval(0);
  screenshot_manager()->Reset();
  ExecuteSyncJSFunction(main_frame, "use_replace_state()");
  screenshot_manager()->WaitUntilScreenshotIsReady();
  // history.replaceState shouldn't capture a screenshot
  EXPECT_FALSE(screenshot_manager()->screenshot_taken_for());
  screenshot_manager()->Reset();
  web_contents->GetController().Reload(true);
  WaitForLoadStop(web_contents);
  // reloading the page shouldn't capture a screenshot
  // TODO (mfomitchev): currently broken. Uncomment when
  // FrameHostMsg_DidCommitProvisionalLoad_Params.was_within_same_page
  // is populated properly when reloading the page.
  //EXPECT_FALSE(screenshot_manager()->screenshot_taken_for());
  screenshot_manager()->Reset();
  ExecuteSyncJSFunction(main_frame, "use_push_state()");
  screenshot_manager()->WaitUntilScreenshotIsReady();
  // pushing a state shouldn't capture a screenshot
  // TODO (mfomitchev): currently broken. Uncomment when
  // FrameHostMsg_DidCommitProvisionalLoad_Params.was_within_same_page
  // is populated properly when pushState is used.
  //EXPECT_FALSE(screenshot_manager()->screenshot_taken_for());
}

// TODO(sadrul): This test is disabled because it reparents in a way the
//               FocusController does not support. This code would crash in
//               a production build. It only passed prior to this revision
//               because testing used the old FocusManager which did some
//               different (osbolete) processing. TODO(sadrul) to figure out
//               how this test should work that mimics production code a bit
//               better.
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest,
                       DISABLED_ContentWindowReparent) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/overscroll_navigation.html"));

  std::unique_ptr<aura::Window> window(new aura::Window(NULL));
  window->Init(ui::LAYER_NOT_DRAWN);

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  ExecuteSyncJSFunction(web_contents->GetMainFrame(), "navigate_next()");
  EXPECT_EQ(1, GetCurrentIndex());

  aura::Window* content = web_contents->GetContentNativeView();
  gfx::Rect bounds = content->GetBoundsInRootWindow();
  ui::test::EventGenerator generator(content->GetRootWindow(), content);
  generator.GestureScrollSequence(
      gfx::Point(bounds.x() + 2, bounds.y() + 10),
      gfx::Point(bounds.right() - 10, bounds.y() + 10),
      base::TimeDelta::FromMilliseconds(20),
      1);

  window->AddChild(shell()->web_contents()->GetContentNativeView());
}

IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest, ContentWindowClose) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/overscroll_navigation.html"));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  ExecuteSyncJSFunction(web_contents->GetMainFrame(), "navigate_next()");
  EXPECT_EQ(1, GetCurrentIndex());

  aura::Window* content = web_contents->GetContentNativeView();
  gfx::Rect bounds = content->GetBoundsInRootWindow();
  ui::test::EventGenerator generator(content->GetRootWindow(), content);
  generator.GestureScrollSequence(
      gfx::Point(bounds.x() + 2, bounds.y() + 10),
      gfx::Point(bounds.right() - 10, bounds.y() + 10),
      base::TimeDelta::FromMilliseconds(20),
      1);

  delete web_contents->GetContentNativeView();
}


#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
// This appears to be flaky in the same was as the other overscroll
// tests. Enabling for non-Windows platforms.
// See http://crbug.com/369871.
// For linux, see http://crbug.com/381294
#define MAYBE_RepeatedQuickOverscrollGestures DISABLED_RepeatedQuickOverscrollGestures
#else
#define MAYBE_RepeatedQuickOverscrollGestures RepeatedQuickOverscrollGestures
#endif

IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest,
                       MAYBE_RepeatedQuickOverscrollGestures) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/overscroll_navigation.html"));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  NavigationController& controller = web_contents->GetController();
  RenderFrameHost* main_frame = web_contents->GetMainFrame();
  ExecuteSyncJSFunction(main_frame, "install_touch_handler()");

  // Navigate twice, then navigate back in history once.
  ExecuteSyncJSFunction(main_frame, "navigate_next()");
  ExecuteSyncJSFunction(main_frame, "navigate_next()");
  EXPECT_EQ(2, GetCurrentIndex());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());

  web_contents->GetController().GoBack();
  WaitForLoadStop(web_contents);
  EXPECT_EQ(1, GetCurrentIndex());
  EXPECT_EQ(base::ASCIIToUTF16("Title: #1"), web_contents->GetTitle());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_TRUE(controller.CanGoForward());

  aura::Window* content = web_contents->GetContentNativeView();
  gfx::Rect bounds = content->GetBoundsInRootWindow();
  ui::test::EventGenerator generator(content->GetRootWindow(), content);

  // Do a swipe left to start a forward navigation. Then quickly do a swipe
  // right.
  base::string16 expected_title = base::ASCIIToUTF16("Title: #2");
  content::TitleWatcher title_watcher(web_contents, expected_title);
  NavigationWatcher nav_watcher(web_contents);

  generator.GestureScrollSequence(
      gfx::Point(bounds.right() - 10, bounds.y() + 10),
      gfx::Point(bounds.x() + 2, bounds.y() + 10),
      base::TimeDelta::FromMilliseconds(2000),
      10);
  nav_watcher.WaitUntilNavigationStarts();

  generator.GestureScrollSequence(
      gfx::Point(bounds.x() + 2, bounds.y() + 10),
      gfx::Point(bounds.right() - 10, bounds.y() + 10),
      base::TimeDelta::FromMilliseconds(2000),
      10);
  base::string16 actual_title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(expected_title, actual_title);

  EXPECT_EQ(2, GetCurrentIndex());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

// Verify that hiding a parent of the renderer will hide the content too.
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest, HideContentOnParenHide) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/title1.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  aura::Window* content = web_contents->GetNativeView()->parent();
  EXPECT_TRUE(web_contents->should_normally_be_visible());
  content->Hide();
  EXPECT_FALSE(web_contents->should_normally_be_visible());
  content->Show();
  EXPECT_TRUE(web_contents->should_normally_be_visible());
}

// Ensure that SnapToPhysicalPixelBoundary() is called on WebContentsView parent
// change. This is a regression test for http://crbug.com/388908.
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest, WebContentsViewReparent) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/overscroll_navigation.html"));

  std::unique_ptr<aura::Window> window(new aura::Window(NULL));
  window->Init(ui::LAYER_NOT_DRAWN);

  RenderWidgetHostViewAura* rwhva =
      static_cast<RenderWidgetHostViewAura*>(
          shell()->web_contents()->GetRenderWidgetHostView());
  rwhva->ResetHasSnappedToBoundary();
  EXPECT_FALSE(rwhva->has_snapped_to_boundary());
  window->AddChild(shell()->web_contents()->GetNativeView());
  EXPECT_TRUE(rwhva->has_snapped_to_boundary());
}

// Flaky on some platforms, likely for the same reason as other flaky overscroll
// tests. http://crbug.com/305722
// TODO(tdresser): Re-enable this once eager GR is back on. See
// crbug.com/410280.
#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
#define MAYBE_OverscrollNavigationTouchThrottling \
        DISABLED_OverscrollNavigationTouchThrottling
#else
#define MAYBE_OverscrollNavigationTouchThrottling \
        DISABLED_OverscrollNavigationTouchThrottling
#endif

// Tests that touch moves are not throttled when performing a scroll gesture on
// a non-scrollable area, except during gesture-nav.
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest,
                       MAYBE_OverscrollNavigationTouchThrottling) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/overscroll_navigation.html"));

  AddInputEventMessageFilter();

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  aura::Window* content = web_contents->GetContentNativeView();
  gfx::Rect bounds = content->GetBoundsInRootWindow();
  const int dx = 20;

  ExecuteSyncJSFunction(web_contents->GetMainFrame(),
                        "install_touchmove_handler()");

  WaitAFrame();

  for (int navigated = 0; navigated <= 1; ++navigated) {
    if (navigated) {
      ExecuteSyncJSFunction(web_contents->GetMainFrame(), "navigate_next()");
      ExecuteSyncJSFunction(web_contents->GetMainFrame(),
                            "reset_touchmove_count()");
    }
    // Send touch press.
    SyntheticWebTouchEvent touch;
    touch.PressPoint(bounds.x() + 2, bounds.y() + 10);
    GetRenderWidgetHost()->ForwardTouchEventWithLatencyInfo(touch,
                                                            ui::LatencyInfo());
    filter()->WaitForAck(blink::WebInputEvent::TouchStart);
    WaitAFrame();

    // Assert on the ack, because we'll end up waiting for acks that will never
    // come if this is not true.
    ASSERT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, filter()->last_ack_state());

    // Send first touch move, and then a scroll begin.
    touch.MovePoint(0, bounds.x() + 20 + 1 * dx, bounds.y() + 100);
    GetRenderWidgetHost()->ForwardTouchEventWithLatencyInfo(touch,
                                                            ui::LatencyInfo());
    filter()->WaitForAck(blink::WebInputEvent::TouchMove);
    ASSERT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, filter()->last_ack_state());

    blink::WebGestureEvent scroll_begin =
        SyntheticWebGestureEventBuilder::BuildScrollBegin(
            1, 1, blink::WebGestureDeviceTouchscreen);
    GetRenderWidgetHost()->ForwardGestureEventWithLatencyInfo(
        scroll_begin, ui::LatencyInfo());
    // Scroll begin ignores ack disposition, so don't wait for the ack.
    WaitAFrame();

    // First touchmove already sent, start at 2.
    for (int i = 2; i <= 10; ++i) {
      // Send a touch move, followed by a scroll update
      touch.MovePoint(0, bounds.x() + 20 + i * dx, bounds.y() + 100);
      GetRenderWidgetHost()->ForwardTouchEventWithLatencyInfo(
          touch, ui::LatencyInfo());
      WaitAFrame();

      blink::WebGestureEvent scroll_update =
          SyntheticWebGestureEventBuilder::BuildScrollUpdate(
              dx, 5, 0, blink::WebGestureDeviceTouchscreen);

      GetRenderWidgetHost()->ForwardGestureEventWithLatencyInfo(
          scroll_update, ui::LatencyInfo());

      WaitAFrame();
    }

    touch.ReleasePoint(0);
    GetRenderWidgetHost()->ForwardTouchEventWithLatencyInfo(touch,
                                                            ui::LatencyInfo());
    WaitAFrame();

    blink::WebGestureEvent scroll_end;
    scroll_end.type = blink::WebInputEvent::GestureScrollEnd;
    GetRenderWidgetHost()->ForwardGestureEventWithLatencyInfo(
        scroll_end, ui::LatencyInfo());
    WaitAFrame();

    if (!navigated)
      EXPECT_EQ(10, ExecuteScriptAndExtractInt("touchmoveCount"));
    else
      EXPECT_GT(10, ExecuteScriptAndExtractInt("touchmoveCount"));
  }
}

// Test that vertical overscroll updates are sent only when a user overscrolls
// vertically.
#if defined(OS_WIN)
#define MAYBE_VerticalOverscroll DISABLED_VerticalOverscroll
#else
#define MAYBE_VerticalOverscroll VerticalOverscroll
#endif

IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest, MAYBE_VerticalOverscroll) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kScrollEndEffect, "1");

  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("about:blank"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  VerticalOverscrollTracker tracker;
  web_contents->SetDelegate(&tracker);

  // This test triggers a large number of animations. Speed them up to ensure
  // the test completes within its time limit.
  ui::ScopedAnimationDurationScaleMode fast_duration_mode(
      ui::ScopedAnimationDurationScaleMode::FAST_DURATION);

  aura::Window* content = web_contents->GetContentNativeView();
  ui::EventProcessor* dispatcher = content->GetHost()->event_processor();
  gfx::Rect bounds = content->GetBoundsInRootWindow();

  // Overscroll horizontally.
  {
    int kXStep = bounds.width() / 10;
    gfx::Point location(bounds.right() - kXStep, bounds.y() + 5);
    base::TimeTicks timestamp = ui::EventTimeForNow();
    ui::TouchEvent press(ui::ET_TOUCH_PRESSED, location, 0, timestamp);
    ui::EventDispatchDetails details = dispatcher->OnEventFromSource(&press);
    ASSERT_FALSE(details.dispatcher_destroyed);
    WaitAFrame();
    location -= gfx::Vector2d(kXStep, 0);
    timestamp += base::TimeDelta::FromMilliseconds(10);

    while (location.x() > bounds.x() + kXStep) {
      ui::TouchEvent inc(ui::ET_TOUCH_MOVED, location, 0, timestamp);
      details = dispatcher->OnEventFromSource(&inc);
      ASSERT_FALSE(details.dispatcher_destroyed);
      WaitAFrame();
      location -= gfx::Vector2d(10, 0);
      timestamp += base::TimeDelta::FromMilliseconds(10);
    }

    ui::TouchEvent release(ui::ET_TOUCH_RELEASED, location, 0, timestamp);
    details = dispatcher->OnEventFromSource(&release);
    ASSERT_FALSE(details.dispatcher_destroyed);
    WaitAFrame();

    EXPECT_EQ(0, tracker.num_overscroll_updates());
    EXPECT_FALSE(tracker.overscroll_completed());
  }

  // Overscroll vertically.
  {
    tracker.Reset();

    int kYStep = bounds.height() / 10;
    gfx::Point location(bounds.x() + 10, bounds.y() + kYStep);
    base::TimeTicks timestamp = ui::EventTimeForNow();
    ui::TouchEvent press(ui::ET_TOUCH_PRESSED, location, 0, timestamp);
    ui::EventDispatchDetails details = dispatcher->OnEventFromSource(&press);
    ASSERT_FALSE(details.dispatcher_destroyed);
    WaitAFrame();
    location += gfx::Vector2d(0, kYStep);
    timestamp += base::TimeDelta::FromMilliseconds(10);

    while (location.y() < bounds.bottom() - kYStep) {
      ui::TouchEvent inc(ui::ET_TOUCH_MOVED, location, 0, timestamp);
      details = dispatcher->OnEventFromSource(&inc);
      ASSERT_FALSE(details.dispatcher_destroyed);
      WaitAFrame();
      location += gfx::Vector2d(0, kYStep);
      timestamp += base::TimeDelta::FromMilliseconds(10);
    }

    ui::TouchEvent release(ui::ET_TOUCH_RELEASED, location, 0, timestamp);
    details = dispatcher->OnEventFromSource(&release);
    ASSERT_FALSE(details.dispatcher_destroyed);
    WaitAFrame();

    EXPECT_LT(0, tracker.num_overscroll_updates());
    EXPECT_TRUE(tracker.overscroll_completed());
  }

  // Start out overscrolling vertically, then switch directions and finish
  // overscrolling horizontally.
  {
    tracker.Reset();

    int kXStep = bounds.width() / 10;
    int kYStep = bounds.height() / 10;
    gfx::Point location = bounds.origin() + gfx::Vector2d(0, kYStep);
    base::TimeTicks timestamp = ui::EventTimeForNow();
    ui::TouchEvent press(ui::ET_TOUCH_PRESSED, location, 0, timestamp);
    ui::EventDispatchDetails details = dispatcher->OnEventFromSource(&press);
    ASSERT_FALSE(details.dispatcher_destroyed);
    WaitAFrame();
    location += gfx::Vector2d(0, kYStep);
    timestamp += base::TimeDelta::FromMilliseconds(10);

    for (size_t i = 0; i < 3; ++i) {
      ui::TouchEvent inc(ui::ET_TOUCH_MOVED, location, 0, timestamp);
      details = dispatcher->OnEventFromSource(&inc);
      ASSERT_FALSE(details.dispatcher_destroyed);
      WaitAFrame();
      location += gfx::Vector2d(0, kYStep);
      timestamp += base::TimeDelta::FromMilliseconds(10);
    }

    while (location.x() < bounds.right() - kXStep) {
      ui::TouchEvent inc(ui::ET_TOUCH_MOVED, location, 0, timestamp);
      details = dispatcher->OnEventFromSource(&inc);
      ASSERT_FALSE(details.dispatcher_destroyed);
      WaitAFrame();
      location += gfx::Vector2d(kXStep, 0);
      timestamp += base::TimeDelta::FromMilliseconds(10);
    }

    ui::TouchEvent release(ui::ET_TOUCH_RELEASED, location, 0, timestamp);
    details = dispatcher->OnEventFromSource(&release);
    ASSERT_FALSE(details.dispatcher_destroyed);
    WaitAFrame();

    EXPECT_LT(0, tracker.num_overscroll_updates());
    EXPECT_FALSE(tracker.overscroll_completed());
  }
}

}  // namespace content
