// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/touch_editable_impl_aura.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_aura.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event_utils.h"

using blink::WebInputEvent;

namespace content {

class TestTouchEditableImplAura : public TouchEditableImplAura {
 public:
  TestTouchEditableImplAura()
      : selection_changed_callback_arrived_(false),
        waiting_for_selection_changed_callback_(false),
        waiting_for_gesture_ack_type_(WebInputEvent::Undefined),
        last_gesture_ack_type_(WebInputEvent::Undefined) {}

  virtual void Reset() {
    selection_changed_callback_arrived_ = false;
    waiting_for_selection_changed_callback_ = false;
    waiting_for_gesture_ack_type_ = WebInputEvent::Undefined;
    last_gesture_ack_type_ = WebInputEvent::Undefined;
  }

  virtual void OnSelectionOrCursorChanged(const gfx::Rect& anchor,
                                          const gfx::Rect& focus) OVERRIDE {
    selection_changed_callback_arrived_ = true;
    TouchEditableImplAura::OnSelectionOrCursorChanged(anchor, focus);
    if (waiting_for_selection_changed_callback_)
      selection_changed_wait_run_loop_->Quit();
  }

  virtual void GestureEventAck(int gesture_event_type) OVERRIDE {
    last_gesture_ack_type_ =
        static_cast<WebInputEvent::Type>(gesture_event_type);
    TouchEditableImplAura::GestureEventAck(gesture_event_type);
    if (waiting_for_gesture_ack_type_ == gesture_event_type)
      gesture_ack_wait_run_loop_->Quit();
  }

  virtual void WaitForSelectionChangeCallback() {
    if (selection_changed_callback_arrived_)
      return;
    waiting_for_selection_changed_callback_ = true;
    selection_changed_wait_run_loop_.reset(new base::RunLoop());
    selection_changed_wait_run_loop_->Run();
  }

  virtual void WaitForGestureAck(WebInputEvent::Type gesture_event_type) {
    if (last_gesture_ack_type_ == gesture_event_type)
      return;
    waiting_for_gesture_ack_type_ = gesture_event_type;
    gesture_ack_wait_run_loop_.reset(new base::RunLoop());
    gesture_ack_wait_run_loop_->Run();
  }

 protected:
  virtual ~TestTouchEditableImplAura() {}

 private:
  bool selection_changed_callback_arrived_;
  bool waiting_for_selection_changed_callback_;
  WebInputEvent::Type waiting_for_gesture_ack_type_;
  WebInputEvent::Type last_gesture_ack_type_;
  scoped_ptr<base::RunLoop> selection_changed_wait_run_loop_;
  scoped_ptr<base::RunLoop> gesture_ack_wait_run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestTouchEditableImplAura);
};

class TouchEditableImplAuraTest : public ContentBrowserTest {
 public:
  TouchEditableImplAuraTest() {}

 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kEnableTouchEditing);
  }

  // Executes the javascript synchronously and makes sure the returned value is
  // freed properly.
  void ExecuteSyncJSFunction(RenderFrameHost* rfh, const std::string& jscript) {
    scoped_ptr<base::Value> value =
        content::ExecuteScriptAndGetValue(rfh, jscript);
  }

  // Starts the test server and navigates to the given url. Sets a large enough
  // size to the root window.  Returns after the navigation to the url is
  // complete.
  void StartTestWithPage(const std::string& url) {
    ASSERT_TRUE(test_server()->Start());
    GURL test_url(test_server()->GetURL(url));
    NavigateToURL(shell(), test_url);
    aura::Window* content = shell()->web_contents()->GetContentNativeView();
    content->GetHost()->SetBounds(gfx::Rect(800, 600));
  }

  RenderWidgetHostViewAura* GetRenderWidgetHostViewAura(
      TouchEditableImplAura* touch_editable) {
    return touch_editable->rwhva_;
  }

  ui::TouchSelectionController* GetTouchSelectionController(
      TouchEditableImplAura* touch_editable) {
    return touch_editable->touch_selection_controller_.get();
  }

  ui::TextInputType GetTextInputType(TouchEditableImplAura* touch_editable) {
    return touch_editable->text_input_type_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchEditableImplAuraTest);
};

IN_PROC_BROWSER_TEST_F(TouchEditableImplAuraTest,
                       TouchSelectionOriginatingFromWebpageTest) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("files/touch_selection.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHost* main_frame = web_contents->GetMainFrame();
  WebContentsViewAura* view_aura = static_cast<WebContentsViewAura*>(
      web_contents->GetView());
  TestTouchEditableImplAura* touch_editable = new TestTouchEditableImplAura;
  view_aura->SetTouchEditableForTest(touch_editable);
  RenderWidgetHostViewAura* rwhva = static_cast<RenderWidgetHostViewAura*>(
      web_contents->GetRenderWidgetHostView());
  aura::Window* content = web_contents->GetContentNativeView();
  aura::test::EventGenerator generator(content->GetRootWindow(), content);
  gfx::Rect bounds = content->GetBoundsInRootWindow();

  touch_editable->Reset();
  ExecuteSyncJSFunction(main_frame, "select_all_text()");
  touch_editable->WaitForSelectionChangeCallback();

  // Tap inside selection to bring up selection handles.
  generator.GestureTapAt(gfx::Point(bounds.x() + 10, bounds.y() + 10));
  EXPECT_EQ(GetRenderWidgetHostViewAura(touch_editable), rwhva);

  scoped_ptr<base::Value> value =
      content::ExecuteScriptAndGetValue(main_frame, "get_selection()");
  std::string selection;
  value->GetAsString(&selection);

  // Check if selection handles are showing.
  EXPECT_TRUE(GetTouchSelectionController(touch_editable));
  EXPECT_STREQ("Some text we can select", selection.c_str());

  // Lets move the handles a bit to modify the selection
  touch_editable->Reset();
  generator.GestureScrollSequence(
      gfx::Point(10, 47),
      gfx::Point(30, 47),
      base::TimeDelta::FromMilliseconds(20),
      5);
  touch_editable->WaitForSelectionChangeCallback();

  EXPECT_TRUE(GetTouchSelectionController(touch_editable));
  value = content::ExecuteScriptAndGetValue(main_frame, "get_selection()");
  value->GetAsString(&selection);

  // It is hard to tell what exactly the selection would be now. But it would
  // definitely be less than whatever was selected before.
  EXPECT_GT(std::strlen("Some text we can select"), selection.size());
}

IN_PROC_BROWSER_TEST_F(TouchEditableImplAuraTest,
                       TestTouchSelectionHiddenWhenScrolling) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("files/touch_selection.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHost* main_frame = web_contents->GetMainFrame();
  WebContentsViewAura* view_aura = static_cast<WebContentsViewAura*>(
      web_contents->GetView());
  TestTouchEditableImplAura* touch_editable = new TestTouchEditableImplAura;
  view_aura->SetTouchEditableForTest(touch_editable);
  RenderWidgetHostViewAura* rwhva = static_cast<RenderWidgetHostViewAura*>(
      web_contents->GetRenderWidgetHostView());
  EXPECT_EQ(GetRenderWidgetHostViewAura(touch_editable), rwhva);

  // Long press to select word.
  ui::GestureEvent long_press(ui::ET_GESTURE_LONG_PRESS,
                              10,
                              10,
                              0,
                              ui::EventTimeForNow(),
                              ui::GestureEventDetails(
                                  ui::ET_GESTURE_LONG_PRESS, 0, 0),
                              1);
  touch_editable->Reset();
  rwhva->OnGestureEvent(&long_press);
  touch_editable->WaitForSelectionChangeCallback();

  // Check if selection handles are showing.
  EXPECT_TRUE(GetTouchSelectionController(touch_editable));

  scoped_ptr<base::Value> value =
      content::ExecuteScriptAndGetValue(main_frame, "get_selection()");
  std::string selection;
  value->GetAsString(&selection);
  EXPECT_STREQ("Some", selection.c_str());

  // Start scrolling. Handles should get hidden.
  ui::GestureEvent scroll_begin(ui::ET_GESTURE_SCROLL_BEGIN,
                                10,
                                10,
                                0,
                                ui::EventTimeForNow(),
                                ui::GestureEventDetails(
                                    ui::ET_GESTURE_SCROLL_BEGIN, 0, 0),
                                1);
  rwhva->OnGestureEvent(&scroll_begin);
  EXPECT_FALSE(GetTouchSelectionController(touch_editable));

  // Handles should come back after scroll ends.
  ui::GestureEvent scroll_end(ui::ET_GESTURE_SCROLL_END,
                              10,
                              10,
                              0,
                              ui::EventTimeForNow(),
                              ui::GestureEventDetails(
                                  ui::ET_GESTURE_SCROLL_END, 0, 0),
                              1);
  rwhva->OnGestureEvent(&scroll_end);
  EXPECT_TRUE(GetTouchSelectionController(touch_editable));
}

IN_PROC_BROWSER_TEST_F(TouchEditableImplAuraTest,
                       TouchSelectionOnLongPressTest) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("files/touch_selection.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHost* main_frame = web_contents->GetMainFrame();
  WebContentsViewAura* view_aura = static_cast<WebContentsViewAura*>(
      web_contents->GetView());
  TestTouchEditableImplAura* touch_editable = new TestTouchEditableImplAura;
  view_aura->SetTouchEditableForTest(touch_editable);
  RenderWidgetHostViewAura* rwhva = static_cast<RenderWidgetHostViewAura*>(
      web_contents->GetRenderWidgetHostView());
  EXPECT_EQ(GetRenderWidgetHostViewAura(touch_editable), rwhva);

  // Long press to select word.
  ui::GestureEvent long_press(ui::ET_GESTURE_LONG_PRESS,
                              10,
                              10,
                              0,
                              ui::EventTimeForNow(),
                              ui::GestureEventDetails(
                                  ui::ET_GESTURE_LONG_PRESS, 0, 0),
                              1);
  touch_editable->Reset();
  rwhva->OnGestureEvent(&long_press);
  touch_editable->WaitForSelectionChangeCallback();

  // Check if selection handles are showing.
  EXPECT_TRUE(GetTouchSelectionController(touch_editable));

  scoped_ptr<base::Value> value =
      content::ExecuteScriptAndGetValue(main_frame, "get_selection()");
  std::string selection;
  value->GetAsString(&selection);
  EXPECT_STREQ("Some", selection.c_str());
}

IN_PROC_BROWSER_TEST_F(TouchEditableImplAuraTest,
                       NoTouchSelectionOnDoubleTapTest) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("files/touch_selection.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHost* main_frame = web_contents->GetMainFrame();
  WebContentsViewAura* view_aura =
      static_cast<WebContentsViewAura*>(web_contents->GetView());
  TestTouchEditableImplAura* touch_editable = new TestTouchEditableImplAura;
  view_aura->SetTouchEditableForTest(touch_editable);
  RenderWidgetHostViewAura* rwhva = static_cast<RenderWidgetHostViewAura*>(
      web_contents->GetRenderWidgetHostView());
  EXPECT_EQ(GetRenderWidgetHostViewAura(touch_editable), rwhva);

  // Double-tap to select word.
  ui::GestureEvent double_tap(ui::ET_GESTURE_TAP,
                              10,
                              10,
                              0,
                              ui::EventTimeForNow(),
                              ui::GestureEventDetails(ui::ET_GESTURE_TAP, 2, 0),
                              1);
  touch_editable->Reset();
  rwhva->OnGestureEvent(&double_tap);
  touch_editable->WaitForSelectionChangeCallback();

  // Make sure touch selection handles are not showing.
  EXPECT_FALSE(GetTouchSelectionController(touch_editable));

  scoped_ptr<base::Value> value =
      content::ExecuteScriptAndGetValue(main_frame, "get_selection()");
  std::string selection;
  value->GetAsString(&selection);
  EXPECT_STREQ("Some", selection.c_str());
}

IN_PROC_BROWSER_TEST_F(TouchEditableImplAuraTest,
                       TouchCursorInTextfieldTest) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("files/touch_selection.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHost* main_frame = web_contents->GetMainFrame();
  WebContentsViewAura* view_aura = static_cast<WebContentsViewAura*>(
      web_contents->GetView());
  TestTouchEditableImplAura* touch_editable = new TestTouchEditableImplAura;
  view_aura->SetTouchEditableForTest(touch_editable);
  RenderWidgetHostViewAura* rwhva = static_cast<RenderWidgetHostViewAura*>(
      web_contents->GetRenderWidgetHostView());
  aura::Window* content = web_contents->GetContentNativeView();
  aura::test::EventGenerator generator(content->GetRootWindow(), content);
  gfx::Rect bounds = content->GetBoundsInRootWindow();
  EXPECT_EQ(GetRenderWidgetHostViewAura(touch_editable), rwhva);

  ExecuteSyncJSFunction(main_frame, "focus_textfield()");
  touch_editable->WaitForSelectionChangeCallback();

  // Tap textfield
  touch_editable->Reset();
  generator.GestureTapAt(gfx::Point(bounds.x() + 50, bounds.y() + 40));
  // Tap Down acks are sent synchronously, while Tap acks are asynchronous.
  touch_editable->WaitForGestureAck(WebInputEvent::GestureTap);
  touch_editable->WaitForSelectionChangeCallback();
  touch_editable->Reset();

  // Check if cursor handle is showing.
  EXPECT_NE(ui::TEXT_INPUT_TYPE_NONE, GetTextInputType(touch_editable));
  EXPECT_TRUE(GetTouchSelectionController(touch_editable));

  scoped_ptr<base::Value> value =
      content::ExecuteScriptAndGetValue(main_frame, "get_cursor_position()");
  int cursor_pos = -1;
  value->GetAsInteger(&cursor_pos);
  EXPECT_NE(-1, cursor_pos);

  // Move the cursor handle.
  generator.GestureScrollSequence(
      gfx::Point(50, 59),
      gfx::Point(10, 59),
      base::TimeDelta::FromMilliseconds(20),
      1);
  touch_editable->WaitForSelectionChangeCallback();
  EXPECT_TRUE(GetTouchSelectionController(touch_editable));
  value = content::ExecuteScriptAndGetValue(main_frame,
                                            "get_cursor_position()");
  int new_cursor_pos = -1;
  value->GetAsInteger(&new_cursor_pos);
  EXPECT_NE(-1, new_cursor_pos);
  // Cursor should have moved.
  EXPECT_NE(new_cursor_pos, cursor_pos);
}

}  // namespace content
