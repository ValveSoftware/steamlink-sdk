// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_controller.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/synthetic_smooth_scroll_gesture.h"
#include "content/browser/renderer_host/input/touch_event_queue.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/common/input_messages.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/event_switches.h"
#include "ui/events/latency_info.h"

using blink::WebInputEvent;

namespace {

const char kTouchActionDataURL[] =
    "data:text/html;charset=utf-8,"
    "<!DOCTYPE html>"
    "<meta name='viewport' content='width=device-width'/>"
    "<style>"
    "html, body {"
    "  margin: 0;"
    "}"
    ".box {"
    "  height: 96px;"
    "  width: 96px;"
    "  border: 2px solid blue;"
    "}"
    ".spacer { height: 1000px; }"
    ".ta-none { touch-action: none; }"
    "</style>"
    "<div class=box></div>"
    "<div class='box ta-none'></div>"
    "<div class=spacer></div>"
    "<script>"
    "  window.eventCounts = "
    "    {touchstart:0, touchmove:0, touchend: 0, touchcancel:0};"
    "  function countEvent(e) { eventCounts[e.type]++; }"
    "  for (var evt in eventCounts) { "
    "    document.addEventListener(evt, countEvent); "
    "  }"
    "  document.title='ready';"
    "</script>";

}  // namespace

namespace content {


class TouchActionBrowserTest : public ContentBrowserTest {
 public:
  TouchActionBrowserTest() {}
  virtual ~TouchActionBrowserTest() {}

  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(shell()->web_contents()->
                                          GetRenderViewHost());
  }

  void OnSyntheticGestureCompleted(SyntheticGesture::Result result) {
    EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
    runner_->Quit();
  }

 protected:
  void LoadURL() {
    const GURL data_url(kTouchActionDataURL);
    NavigateToURL(shell(), data_url);

    RenderWidgetHostImpl* host = GetWidgetHost();
    host->GetView()->SetSize(gfx::Size(400, 400));

    base::string16 ready_title(base::ASCIIToUTF16("ready"));
    TitleWatcher watcher(shell()->web_contents(), ready_title);
    ignore_result(watcher.WaitAndGetTitle());
  }

  // ContentBrowserTest:
  virtual void SetUpCommandLine(CommandLine* cmd) OVERRIDE {
    cmd->AppendSwitchASCII(switches::kTouchEvents,
                           switches::kTouchEventsEnabled);
    // TODO(rbyers): Remove this switch once touch-action ships.
    // http://crbug.com/241964
    cmd->AppendSwitch(switches::kEnableExperimentalWebPlatformFeatures);
  }

  int ExecuteScriptAndExtractInt(const std::string& script) {
    int value = 0;
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        shell()->web_contents(),
        "domAutomationController.send(" + script + ")",
        &value));
    return value;
  }

  int GetScrollTop() {
    return ExecuteScriptAndExtractInt("document.documentElement.scrollTop");
  }

  // Generate touch events for a synthetic scroll from |point| for |distance|.
  // Returns true if the page scrolled by the desired amount, and false if
  // it didn't scroll at all.
  bool DoTouchScroll(const gfx::Point& point, const gfx::Vector2d& distance) {
    EXPECT_EQ(0, GetScrollTop());

    int scrollHeight = ExecuteScriptAndExtractInt(
        "document.documentElement.scrollHeight");
    EXPECT_EQ(1200, scrollHeight);

    SyntheticSmoothScrollGestureParams params;
    params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
    params.anchor = point;
    params.distances.push_back(-distance);

    runner_ = new MessageLoopRunner();

    scoped_ptr<SyntheticSmoothScrollGesture> gesture(
        new SyntheticSmoothScrollGesture(params));
    GetWidgetHost()->QueueSyntheticGesture(
        gesture.PassAs<SyntheticGesture>(),
        base::Bind(&TouchActionBrowserTest::OnSyntheticGestureCompleted,
            base::Unretained(this)));

    // Runs until we get the OnSyntheticGestureCompleted callback
    runner_->Run();
    runner_ = NULL;

    // Check the scroll offset
    int scrollTop = GetScrollTop();
    if (scrollTop == 0)
      return false;

    EXPECT_EQ(distance.y(), scrollTop);
    return true;
  }

 private:
  scoped_refptr<MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(TouchActionBrowserTest);
};

// TouchActionBrowserTest.DefaultAuto fails under ThreadSanitizer v2, see
// http://crbug.com/348539 and is flaky on XP, see
// http://crbug.com/354763
//
// Mac doesn't yet have a gesture recognizer, so can't support turning touch
// events into scroll gestures.
// Will be fixed with http://crbug.com/337142
//
// Verify the test infrastructure works - we can touch-scroll the page and get a
// touchcancel as expected.
IN_PROC_BROWSER_TEST_F(TouchActionBrowserTest, DISABLED_DefaultAuto) {
  LoadURL();

  bool scrolled = DoTouchScroll(gfx::Point(50, 50), gfx::Vector2d(0, 45));
  EXPECT_TRUE(scrolled);

  EXPECT_EQ(1, ExecuteScriptAndExtractInt("eventCounts.touchstart"));
  EXPECT_EQ(1, ExecuteScriptAndExtractInt("eventCounts.touchmove"));
  if (TouchEventQueue::TOUCH_SCROLLING_MODE_DEFAULT ==
      TouchEventQueue::TOUCH_SCROLLING_MODE_TOUCHCANCEL) {
    EXPECT_EQ(0, ExecuteScriptAndExtractInt("eventCounts.touchend"));
    EXPECT_EQ(1, ExecuteScriptAndExtractInt("eventCounts.touchcancel"));
  } else {
    EXPECT_EQ(1, ExecuteScriptAndExtractInt("eventCounts.touchend"));
    EXPECT_EQ(0, ExecuteScriptAndExtractInt("eventCounts.touchcancel"));
  }
}

// Verify that touching a touch-action: none region disables scrolling and
// enables all touch events to be sent.
// Disabled on MacOS because it doesn't support touch input.
// Flaky on OS_CHROMEOS http://crbug.com/376695.
// Also flaky on Linux Tests (TSan v2) http://crbug.com/376668.
#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
#define MAYBE_TouchActionNone DISABLED_TouchActionNone
#elif defined(THREAD_SANITIZER) && defined(OS_LINUX)
#define MAYBE_TouchActionNone DISABLED_TouchActionNone
#else
#define MAYBE_TouchActionNone TouchActionNone
#endif
IN_PROC_BROWSER_TEST_F(TouchActionBrowserTest, MAYBE_TouchActionNone) {
  LoadURL();

  bool scrolled = DoTouchScroll(gfx::Point(50, 150), gfx::Vector2d(0, 45));
  EXPECT_FALSE(scrolled);

  EXPECT_EQ(1, ExecuteScriptAndExtractInt("eventCounts.touchstart"));
  EXPECT_GT(ExecuteScriptAndExtractInt("eventCounts.touchmove"), 1);
  EXPECT_EQ(1, ExecuteScriptAndExtractInt("eventCounts.touchend"));
  EXPECT_EQ(0, ExecuteScriptAndExtractInt("eventCounts.touchcancel"));
}

}  // namespace content
