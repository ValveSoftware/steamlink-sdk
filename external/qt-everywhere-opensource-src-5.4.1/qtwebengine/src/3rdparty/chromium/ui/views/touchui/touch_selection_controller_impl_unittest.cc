// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/touch/touch_editing_controller.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/render_text.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/touchui/touch_selection_controller_impl.h"
#include "ui/views/views_touch_selection_controller_factory.h"
#include "ui/views/widget/widget.h"

using base::ASCIIToUTF16;
using base::UTF16ToUTF8;
using base::WideToUTF16;

namespace {
// Should match kSelectionHandlePadding in touch_selection_controller.
const int kPadding = 10;

// Should match kSelectionHandleBarMinHeight in touch_selection_controller.
const int kBarMinHeight = 5;

// Should match kSelectionHandleBarBottomAllowance in
// touch_selection_controller.
const int kBarBottomAllowance = 3;

// Should match kMenuButtonWidth in touch_editing_menu.
const int kMenuButtonWidth = 63;

// Should match size of kMenuCommands array in touch_editing_menu.
const int kMenuCommandCount = 3;

gfx::Image* GetHandleImage() {
  static gfx::Image* handle_image = NULL;
  if (!handle_image) {
    handle_image = &ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_TEXT_SELECTION_HANDLE);
  }
  return handle_image;
}

gfx::Size GetHandleImageSize() {
  return GetHandleImage()->Size();
}
}  // namespace

namespace views {

class TouchSelectionControllerImplTest : public ViewsTestBase {
 public:
  TouchSelectionControllerImplTest()
      : textfield_widget_(NULL),
        widget_(NULL),
        textfield_(NULL),
        views_tsc_factory_(new ViewsTouchSelectionControllerFactory) {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableTouchEditing);
    ui::TouchSelectionControllerFactory::SetInstance(views_tsc_factory_.get());
  }

  virtual ~TouchSelectionControllerImplTest() {
    ui::TouchSelectionControllerFactory::SetInstance(NULL);
  }

  virtual void TearDown() {
    if (textfield_widget_ && !textfield_widget_->IsClosed())
      textfield_widget_->Close();
    if (widget_ && !widget_->IsClosed())
      widget_->Close();
    ViewsTestBase::TearDown();
  }

  void CreateTextfield() {
    textfield_ = new Textfield();
    textfield_widget_ = new Widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    textfield_widget_->Init(params);
    View* container = new View();
    textfield_widget_->SetContentsView(container);
    container->AddChildView(textfield_);

    textfield_->SetBoundsRect(gfx::Rect(0, 0, 200, 20));
    textfield_->set_id(1);
    textfield_widget_->Show();

    textfield_->RequestFocus();

    textfield_test_api_.reset(new TextfieldTestApi(textfield_));
  }

  void CreateWidget() {
    widget_ = new Widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    widget_->Init(params);
    widget_->Show();
  }

 protected:
  static bool IsCursorHandleVisibleFor(
      ui::TouchSelectionController* controller) {
    TouchSelectionControllerImpl* impl =
        static_cast<TouchSelectionControllerImpl*>(controller);
    return impl->IsCursorHandleVisible();
  }

  gfx::Rect GetCursorRect(const gfx::SelectionModel& sel) {
    return textfield_test_api_->GetRenderText()->GetCursorBounds(sel, true);
  }

  gfx::Point GetCursorPosition(const gfx::SelectionModel& sel) {
    gfx::Rect cursor_bounds = GetCursorRect(sel);
    return gfx::Point(cursor_bounds.x(), cursor_bounds.y());
  }

  TouchSelectionControllerImpl* GetSelectionController() {
    return static_cast<TouchSelectionControllerImpl*>(
        textfield_test_api_->touch_selection_controller());
  }

  void StartTouchEditing() {
    textfield_test_api_->CreateTouchSelectionControllerAndNotifyIt();
  }

  void EndTouchEditing() {
    textfield_test_api_->ResetTouchSelectionController();
  }

  void SimulateSelectionHandleDrag(gfx::Point p, int selection_handle) {
    TouchSelectionControllerImpl* controller = GetSelectionController();
    // Do the work of OnMousePressed().
    if (selection_handle == 1)
      controller->SetDraggingHandle(controller->selection_handle_1_.get());
    else
      controller->SetDraggingHandle(controller->selection_handle_2_.get());

    // Offset the drag position by the selection handle radius since it is
    // supposed to be in the coordinate system of the handle.
    p.Offset(GetHandleImageSize().width() / 2 + kPadding, 0);
    controller->SelectionHandleDragged(p);

    // Do the work of OnMouseReleased().
    controller->dragging_handle_ = NULL;
  }

  gfx::NativeView GetCursorHandleNativeView() {
    return GetSelectionController()->GetCursorHandleNativeView();
  }

  gfx::Point GetSelectionHandle1Position() {
    return GetSelectionController()->GetSelectionHandle1Position();
  }

  gfx::Point GetSelectionHandle2Position() {
    return GetSelectionController()->GetSelectionHandle2Position();
  }

  gfx::Point GetCursorHandlePosition() {
    return GetSelectionController()->GetCursorHandlePosition();
  }

  bool IsSelectionHandle1Visible() {
    return GetSelectionController()->IsSelectionHandle1Visible();
  }

  bool IsSelectionHandle2Visible() {
    return GetSelectionController()->IsSelectionHandle2Visible();
  }

  bool IsCursorHandleVisible() {
    return GetSelectionController()->IsCursorHandleVisible();
  }

  gfx::RenderText* GetRenderText() {
    return textfield_test_api_->GetRenderText();
  }

  gfx::Point GetCursorHandleDragPoint() {
    gfx::Point point = GetCursorHandlePosition();
    const gfx::SelectionModel& sel = textfield_->GetSelectionModel();
    int cursor_height = GetCursorRect(sel).height();
    point.Offset(GetHandleImageSize().width() / 2 + kPadding,
                 GetHandleImageSize().height() / 2 + cursor_height);
    return point;
  }

  Widget* textfield_widget_;
  Widget* widget_;

  Textfield* textfield_;
  scoped_ptr<TextfieldTestApi> textfield_test_api_;
  scoped_ptr<ViewsTouchSelectionControllerFactory> views_tsc_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchSelectionControllerImplTest);
};

// If textfield has selection, this macro verifies that the selection handles
// are visible and at the correct positions (at the end points of selection).
// |cursor_at_selection_handle_1| is used to decide whether selection
// handle 1's position is matched against the start of selection or the end.
#define VERIFY_HANDLE_POSITIONS(cursor_at_selection_handle_1)                  \
{                                                                              \
    gfx::SelectionModel sel = textfield_->GetSelectionModel();                 \
    if (textfield_->HasSelection()) {                                          \
      EXPECT_TRUE(IsSelectionHandle1Visible());                                \
      EXPECT_TRUE(IsSelectionHandle2Visible());                                \
      EXPECT_FALSE(IsCursorHandleVisible());                                   \
      gfx::SelectionModel sel_start = GetRenderText()->                        \
                                      GetSelectionModelForSelectionStart();    \
      gfx::Point selection_start = GetCursorPosition(sel_start);               \
      gfx::Point selection_end = GetCursorPosition(sel);                       \
      gfx::Point sh1 = GetSelectionHandle1Position();                          \
      gfx::Point sh2 = GetSelectionHandle2Position();                          \
      sh1.Offset(GetHandleImageSize().width() / 2 + kPadding, 0);              \
      sh2.Offset(GetHandleImageSize().width() / 2 + kPadding, 0);              \
      if (cursor_at_selection_handle_1) {                                      \
        EXPECT_EQ(sh1, selection_end);                                         \
        EXPECT_EQ(sh2, selection_start);                                       \
      } else {                                                                 \
        EXPECT_EQ(sh1, selection_start);                                       \
        EXPECT_EQ(sh2, selection_end);                                         \
      }                                                                        \
    } else {                                                                   \
      EXPECT_FALSE(IsSelectionHandle1Visible());                               \
      EXPECT_FALSE(IsSelectionHandle2Visible());                               \
      EXPECT_TRUE(IsCursorHandleVisible());                                    \
      gfx::Point cursor_pos = GetCursorPosition(sel);                          \
      gfx::Point ch_pos = GetCursorHandlePosition();                           \
      ch_pos.Offset(GetHandleImageSize().width() / 2 + kPadding, 0);           \
      EXPECT_EQ(ch_pos, cursor_pos);                                           \
    }                                                                          \
}

// Tests that the selection handles are placed appropriately when selection in
// a Textfield changes.
TEST_F(TouchSelectionControllerImplTest, SelectionInTextfieldTest) {
  CreateTextfield();
  textfield_->SetText(ASCIIToUTF16("some text"));
  // Tap the textfield to invoke touch selection.
  ui::GestureEvent tap(ui::ET_GESTURE_TAP, 0, 0, 0, base::TimeDelta(),
      ui::GestureEventDetails(ui::ET_GESTURE_TAP, 1.0f, 0.0f), 0);
  textfield_->OnGestureEvent(&tap);

  // Test selecting a range.
  textfield_->SelectRange(gfx::Range(3, 7));
  VERIFY_HANDLE_POSITIONS(false);

  // Test selecting everything.
  textfield_->SelectAll(false);
  VERIFY_HANDLE_POSITIONS(false);

  // Test with no selection.
  textfield_->ClearSelection();
  VERIFY_HANDLE_POSITIONS(false);

  // Test with lost focus.
  textfield_widget_->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(GetSelectionController());

  // Test with focus re-gained.
  textfield_widget_->GetFocusManager()->SetFocusedView(textfield_);
  EXPECT_FALSE(GetSelectionController());
  textfield_->OnGestureEvent(&tap);
  VERIFY_HANDLE_POSITIONS(false);
}

// Tests that the selection handles are placed appropriately in bidi text.
TEST_F(TouchSelectionControllerImplTest, SelectionInBidiTextfieldTest) {
  CreateTextfield();
  textfield_->SetText(WideToUTF16(L"abc\x05d0\x05d1\x05d2"));
  // Tap the textfield to invoke touch selection.
  ui::GestureEvent tap(ui::ET_GESTURE_TAP, 0, 0, 0, base::TimeDelta(),
      ui::GestureEventDetails(ui::ET_GESTURE_TAP, 1.0f, 0.0f), 0);
  textfield_->OnGestureEvent(&tap);

  // Test cursor at run boundary and with empty selection.
  textfield_->SelectSelectionModel(
      gfx::SelectionModel(3, gfx::CURSOR_BACKWARD));
  VERIFY_HANDLE_POSITIONS(false);

  // Test selection range inside one run and starts or ends at run boundary.
  textfield_->SelectRange(gfx::Range(2, 3));
  VERIFY_HANDLE_POSITIONS(false);

  textfield_->SelectRange(gfx::Range(3, 2));
  VERIFY_HANDLE_POSITIONS(false);

  textfield_->SelectRange(gfx::Range(3, 4));
  VERIFY_HANDLE_POSITIONS(false);

  textfield_->SelectRange(gfx::Range(4, 3));
  VERIFY_HANDLE_POSITIONS(false);

  textfield_->SelectRange(gfx::Range(3, 6));
  VERIFY_HANDLE_POSITIONS(false);

  textfield_->SelectRange(gfx::Range(6, 3));
  VERIFY_HANDLE_POSITIONS(false);

  // Test selection range accross runs.
  textfield_->SelectRange(gfx::Range(0, 6));
  VERIFY_HANDLE_POSITIONS(false);

  textfield_->SelectRange(gfx::Range(6, 0));
  VERIFY_HANDLE_POSITIONS(false);

  textfield_->SelectRange(gfx::Range(1, 4));
  VERIFY_HANDLE_POSITIONS(false);

  textfield_->SelectRange(gfx::Range(4, 1));
  VERIFY_HANDLE_POSITIONS(false);
}

// Tests if the SelectRect callback is called appropriately when selection
// handles are moved.
TEST_F(TouchSelectionControllerImplTest, SelectRectCallbackTest) {
  CreateTextfield();
  textfield_->SetText(ASCIIToUTF16("textfield with selected text"));
  // Tap the textfield to invoke touch selection.
  ui::GestureEvent tap(ui::ET_GESTURE_TAP, 0, 0, 0, base::TimeDelta(),
      ui::GestureEventDetails(ui::ET_GESTURE_TAP, 1.0f, 0.0f), 0);
  textfield_->OnGestureEvent(&tap);
  textfield_->SelectRange(gfx::Range(3, 7));

  EXPECT_EQ(UTF16ToUTF8(textfield_->GetSelectedText()), "tfie");
  VERIFY_HANDLE_POSITIONS(false);

  // Drag selection handle 2 to right by 3 chars.
  const gfx::FontList& font_list = textfield_->GetFontList();
  int x = gfx::Canvas::GetStringWidth(ASCIIToUTF16("ld "), font_list);
  SimulateSelectionHandleDrag(gfx::Point(x, 0), 2);
  EXPECT_EQ(UTF16ToUTF8(textfield_->GetSelectedText()), "tfield ");
  VERIFY_HANDLE_POSITIONS(false);

  // Drag selection handle 1 to the left by a large amount (selection should
  // just stick to the beginning of the textfield).
  SimulateSelectionHandleDrag(gfx::Point(-50, 0), 1);
  EXPECT_EQ(UTF16ToUTF8(textfield_->GetSelectedText()), "textfield ");
  VERIFY_HANDLE_POSITIONS(true);

  // Drag selection handle 1 across selection handle 2.
  x = gfx::Canvas::GetStringWidth(ASCIIToUTF16("textfield with "), font_list);
  SimulateSelectionHandleDrag(gfx::Point(x, 0), 1);
  EXPECT_EQ(UTF16ToUTF8(textfield_->GetSelectedText()), "with ");
  VERIFY_HANDLE_POSITIONS(true);

  // Drag selection handle 2 across selection handle 1.
  x = gfx::Canvas::GetStringWidth(ASCIIToUTF16("with selected "), font_list);
  SimulateSelectionHandleDrag(gfx::Point(x, 0), 2);
  EXPECT_EQ(UTF16ToUTF8(textfield_->GetSelectedText()), "selected ");
  VERIFY_HANDLE_POSITIONS(false);
}

TEST_F(TouchSelectionControllerImplTest, SelectRectInBidiCallbackTest) {
  CreateTextfield();
  textfield_->SetText(WideToUTF16(L"abc\x05e1\x05e2\x05e3" L"def"));
  // Tap the textfield to invoke touch selection.
  ui::GestureEvent tap(ui::ET_GESTURE_TAP, 0, 0, 0, base::TimeDelta(),
      ui::GestureEventDetails(ui::ET_GESTURE_TAP, 1.0f, 0.0f), 0);
  textfield_->OnGestureEvent(&tap);

  // Select [c] from left to right.
  textfield_->SelectRange(gfx::Range(2, 3));
  EXPECT_EQ(WideToUTF16(L"c"), textfield_->GetSelectedText());
  VERIFY_HANDLE_POSITIONS(false);

  // Drag selection handle 2 to right by 1 char.
  const gfx::FontList& font_list = textfield_->GetFontList();
  int x = gfx::Canvas::GetStringWidth(WideToUTF16(L"\x05e3"), font_list);
  SimulateSelectionHandleDrag(gfx::Point(x, 0), 2);
  EXPECT_EQ(WideToUTF16(L"c\x05e1\x05e2"), textfield_->GetSelectedText());
  VERIFY_HANDLE_POSITIONS(false);

  // Drag selection handle 1 to left by 1 char.
  x = gfx::Canvas::GetStringWidth(WideToUTF16(L"b"), font_list);
  SimulateSelectionHandleDrag(gfx::Point(-x, 0), 1);
  EXPECT_EQ(WideToUTF16(L"bc\x05e1\x05e2"), textfield_->GetSelectedText());
  VERIFY_HANDLE_POSITIONS(true);

  // Select [c] from right to left.
  textfield_->SelectRange(gfx::Range(3, 2));
  EXPECT_EQ(WideToUTF16(L"c"), textfield_->GetSelectedText());
  VERIFY_HANDLE_POSITIONS(false);

  // Drag selection handle 1 to right by 1 char.
  x = gfx::Canvas::GetStringWidth(WideToUTF16(L"\x05e3"), font_list);
  SimulateSelectionHandleDrag(gfx::Point(x, 0), 1);
  EXPECT_EQ(WideToUTF16(L"c\x05e1\x05e2"), textfield_->GetSelectedText());
  VERIFY_HANDLE_POSITIONS(true);

  // Drag selection handle 2 to left by 1 char.
  x = gfx::Canvas::GetStringWidth(WideToUTF16(L"b"), font_list);
  SimulateSelectionHandleDrag(gfx::Point(-x, 0), 2);
  EXPECT_EQ(WideToUTF16(L"bc\x05e1\x05e2"), textfield_->GetSelectedText());
  VERIFY_HANDLE_POSITIONS(false);

  // Select [\x5e1] from right to left.
  textfield_->SelectRange(gfx::Range(3, 4));
  EXPECT_EQ(WideToUTF16(L"\x05e1"), textfield_->GetSelectedText());
  VERIFY_HANDLE_POSITIONS(false);

  /* TODO(xji): for bidi text "abcDEF" whose display is "abcFEDhij", when click
     right of 'D' and select [D] then move the left selection handle to left
     by one character, it should select [ED], instead it selects [F].
     Reason: click right of 'D' and left of 'h' return the same x-axis position,
     pass this position to FindCursorPosition() returns index of 'h'. which
     means the selection start changed from 3 to 6.
     Need further investigation on whether this is a bug in Pango and how to
     work around it.
  // Drag selection handle 2 to left by 1 char.
  x = gfx::Canvas::GetStringWidth(WideToUTF16(L"\x05e2"), font_list);
  SimulateSelectionHandleDrag(gfx::Point(-x, 0), 2);
  EXPECT_EQ(WideToUTF16(L"\x05e1\x05e2"), textfield_->GetSelectedText());
  VERIFY_HANDLE_POSITIONS(false);
  */

  // Drag selection handle 1 to right by 1 char.
  x = gfx::Canvas::GetStringWidth(WideToUTF16(L"d"), font_list);
  SimulateSelectionHandleDrag(gfx::Point(x, 0), 1);
  EXPECT_EQ(WideToUTF16(L"\x05e2\x05e3" L"d"), textfield_->GetSelectedText());
  VERIFY_HANDLE_POSITIONS(true);

  // Select [\x5e1] from left to right.
  textfield_->SelectRange(gfx::Range(4, 3));
  EXPECT_EQ(WideToUTF16(L"\x05e1"), textfield_->GetSelectedText());
  VERIFY_HANDLE_POSITIONS(false);

  /* TODO(xji): see detail of above commented out test case.
  // Drag selection handle 1 to left by 1 char.
  x = gfx::Canvas::GetStringWidth(WideToUTF16(L"\x05e2"), font_list);
  SimulateSelectionHandleDrag(gfx::Point(-x, 0), 1);
  EXPECT_EQ(WideToUTF16(L"\x05e1\x05e2"), textfield_->GetSelectedText());
  VERIFY_HANDLE_POSITIONS(true);
  */

  // Drag selection handle 2 to right by 1 char.
  x = gfx::Canvas::GetStringWidth(WideToUTF16(L"d"), font_list);
  SimulateSelectionHandleDrag(gfx::Point(x, 0), 2);
  EXPECT_EQ(WideToUTF16(L"\x05e2\x05e3" L"d"), textfield_->GetSelectedText());
  VERIFY_HANDLE_POSITIONS(false);

  // Select [\x05r3] from right to left.
  textfield_->SelectRange(gfx::Range(5, 6));
  EXPECT_EQ(WideToUTF16(L"\x05e3"), textfield_->GetSelectedText());
  VERIFY_HANDLE_POSITIONS(false);

  // Drag selection handle 2 to left by 1 char.
  x = gfx::Canvas::GetStringWidth(WideToUTF16(L"c"), font_list);
  SimulateSelectionHandleDrag(gfx::Point(-x, 0), 2);
  EXPECT_EQ(WideToUTF16(L"c\x05e1\x05e2"), textfield_->GetSelectedText());
  VERIFY_HANDLE_POSITIONS(false);

  // Drag selection handle 1 to right by 1 char.
  x = gfx::Canvas::GetStringWidth(WideToUTF16(L"\x05e2"), font_list);
  SimulateSelectionHandleDrag(gfx::Point(x, 0), 1);
  EXPECT_EQ(WideToUTF16(L"c\x05e1"), textfield_->GetSelectedText());
  VERIFY_HANDLE_POSITIONS(true);

  // Select [\x05r3] from left to right.
  textfield_->SelectRange(gfx::Range(6, 5));
  EXPECT_EQ(WideToUTF16(L"\x05e3"), textfield_->GetSelectedText());
  VERIFY_HANDLE_POSITIONS(false);

  // Drag selection handle 1 to left by 1 char.
  x = gfx::Canvas::GetStringWidth(WideToUTF16(L"c"), font_list);
  SimulateSelectionHandleDrag(gfx::Point(-x, 0), 1);
  EXPECT_EQ(WideToUTF16(L"c\x05e1\x05e2"), textfield_->GetSelectedText());
  VERIFY_HANDLE_POSITIONS(true);

  // Drag selection handle 2 to right by 1 char.
  x = gfx::Canvas::GetStringWidth(WideToUTF16(L"\x05e2"), font_list);
  SimulateSelectionHandleDrag(gfx::Point(x, 0), 2);
  EXPECT_EQ(WideToUTF16(L"c\x05e1"), textfield_->GetSelectedText());
  VERIFY_HANDLE_POSITIONS(false);
}

TEST_F(TouchSelectionControllerImplTest,
       HiddenSelectionHandleRetainsCursorPosition) {
  // Create a textfield with lots of text in it.
  CreateTextfield();
  std::string textfield_text("some text");
  for (int i = 0; i < 10; ++i)
    textfield_text += textfield_text;
  textfield_->SetText(ASCIIToUTF16(textfield_text));

  // Tap the textfield to invoke selection.
  ui::GestureEvent tap(ui::ET_GESTURE_TAP, 0, 0, 0, base::TimeDelta(),
      ui::GestureEventDetails(ui::ET_GESTURE_TAP, 1.0f, 0.0f), 0);
  textfield_->OnGestureEvent(&tap);

  // Select some text such that one handle is hidden.
  textfield_->SelectRange(gfx::Range(10, textfield_text.length()));

  // Check that one selection handle is hidden.
  EXPECT_FALSE(IsSelectionHandle1Visible());
  EXPECT_TRUE(IsSelectionHandle2Visible());
  EXPECT_EQ(gfx::Range(10, textfield_text.length()),
            textfield_->GetSelectedRange());

  // Drag the visible handle around and make sure the selection end point of the
  // invisible handle does not change.
  size_t visible_handle_position = textfield_->GetSelectedRange().end();
  for (int i = 0; i < 10; ++i) {
    SimulateSelectionHandleDrag(gfx::Point(-10, 0), 2);
    // Make sure that the visible handle is being dragged.
    EXPECT_NE(visible_handle_position, textfield_->GetSelectedRange().end());
    visible_handle_position = textfield_->GetSelectedRange().end();
    EXPECT_EQ((size_t) 10, textfield_->GetSelectedRange().start());
  }
}

TEST_F(TouchSelectionControllerImplTest,
       DoubleTapInTextfieldWithCursorHandleShouldSelectText) {
  CreateTextfield();
  textfield_->SetText(ASCIIToUTF16("some text"));
  aura::test::EventGenerator generator(
      textfield_->GetWidget()->GetNativeView()->GetRootWindow());

  // Tap the textfield to invoke touch selection.
  generator.GestureTapAt(gfx::Point(10, 10));

  // Cursor handle should be visible.
  EXPECT_FALSE(textfield_->HasSelection());
  VERIFY_HANDLE_POSITIONS(false);

  // Double tap on the cursor handle position. We want to check that the cursor
  // handle is not eating the event and that the event is falling through to the
  // textfield.
  gfx::Point cursor_pos = GetCursorHandlePosition();
  cursor_pos.Offset(GetHandleImageSize().width() / 2 + kPadding, 0);
  generator.GestureTapAt(cursor_pos);
  generator.GestureTapAt(cursor_pos);
  EXPECT_TRUE(textfield_->HasSelection());
}

// A simple implementation of TouchEditable that allows faking cursor position
// inside its boundaries.
class TestTouchEditable : public ui::TouchEditable {
 public:
  explicit TestTouchEditable(aura::Window* window)
      : window_(window) {
    DCHECK(window);
  }

  void set_bounds(const gfx::Rect& bounds) {
    bounds_ = bounds;
  }

  void set_cursor_rect(const gfx::Rect& cursor_rect) {
    cursor_rect_ = cursor_rect;
  }

  virtual ~TestTouchEditable() {}

 private:
  // Overridden from ui::TouchEditable.
  virtual void SelectRect(
      const gfx::Point& start, const gfx::Point& end) OVERRIDE {
    NOTREACHED();
  }
  virtual void MoveCaretTo(const gfx::Point& point) OVERRIDE {
    NOTREACHED();
  }
  virtual void GetSelectionEndPoints(gfx::Rect* p1, gfx::Rect* p2) OVERRIDE {
    *p1 = *p2 = cursor_rect_;
  }
  virtual gfx::Rect GetBounds() OVERRIDE {
    return gfx::Rect(bounds_.size());
  }
  virtual gfx::NativeView GetNativeView() const OVERRIDE {
    return window_;
  }
  virtual void ConvertPointToScreen(gfx::Point* point) OVERRIDE {
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(window_->GetRootWindow());
    if (screen_position_client)
      screen_position_client->ConvertPointToScreen(window_, point);
  }
  virtual void ConvertPointFromScreen(gfx::Point* point) OVERRIDE {
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(window_->GetRootWindow());
    if (screen_position_client)
      screen_position_client->ConvertPointFromScreen(window_, point);
  }
  virtual bool DrawsHandles() OVERRIDE {
    return false;
  }
  virtual void OpenContextMenu(const gfx::Point& anchor) OVERRIDE {
    NOTREACHED();
  }
  virtual void DestroyTouchSelection() OVERRIDE {
    NOTREACHED();
  }

  // Overridden from ui::SimpleMenuModel::Delegate.
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE {
    NOTREACHED();
    return false;
  }
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE {
    NOTREACHED();
    return false;
  }
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE {
    NOTREACHED();
    return false;
  }
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE {
    NOTREACHED();
  }

  aura::Window* window_;

  // Boundaries of the client view.
  gfx::Rect bounds_;

  // Cursor position inside the client view.
  gfx::Rect cursor_rect_;

  DISALLOW_COPY_AND_ASSIGN(TestTouchEditable);
};

// Tests if the touch editing handle is shown or hidden properly according to
// the cursor position relative to the client boundaries.
TEST_F(TouchSelectionControllerImplTest,
       VisibilityOfHandleRegardingClientBounds) {
  CreateWidget();

  TestTouchEditable touch_editable(widget_->GetNativeView());
  scoped_ptr<ui::TouchSelectionController> touch_selection_controller(
      ui::TouchSelectionController::create(&touch_editable));

  touch_editable.set_bounds(gfx::Rect(0, 0, 100, 20));

  // Put the cursor completely inside the client bounds. Handle should be
  // visible.
  touch_editable.set_cursor_rect(gfx::Rect(2, 0, 1, 20));
  touch_selection_controller->SelectionChanged();
  EXPECT_TRUE(IsCursorHandleVisibleFor(touch_selection_controller.get()));

  // Move the cursor up such that |kBarMinHeight| pixels are still in the client
  // bounds. Handle should still be visible.
  touch_editable.set_cursor_rect(gfx::Rect(2, kBarMinHeight - 20, 1, 20));
  touch_selection_controller->SelectionChanged();
  EXPECT_TRUE(IsCursorHandleVisibleFor(touch_selection_controller.get()));

  // Move the cursor up such that less than |kBarMinHeight| pixels are in the
  // client bounds. Handle should be hidden.
  touch_editable.set_cursor_rect(gfx::Rect(2, kBarMinHeight - 20 - 1, 1, 20));
  touch_selection_controller->SelectionChanged();
  EXPECT_FALSE(IsCursorHandleVisibleFor(touch_selection_controller.get()));

  // Move the Cursor down such that |kBarBottomAllowance| pixels are out of the
  // client bounds. Handle should be visible.
  touch_editable.set_cursor_rect(gfx::Rect(2, kBarBottomAllowance, 1, 20));
  touch_selection_controller->SelectionChanged();
  EXPECT_TRUE(IsCursorHandleVisibleFor(touch_selection_controller.get()));

  // Move the cursor down such that more than |kBarBottomAllowance| pixels are
  // out of the client bounds. Handle should be hidden.
  touch_editable.set_cursor_rect(gfx::Rect(2, kBarBottomAllowance + 1, 1, 20));
  touch_selection_controller->SelectionChanged();
  EXPECT_FALSE(IsCursorHandleVisibleFor(touch_selection_controller.get()));

  touch_selection_controller.reset();
}

TEST_F(TouchSelectionControllerImplTest, HandlesStackAboveParent) {
  ui::EventTarget* root = GetContext();
  ui::EventTargeter* targeter = root->GetEventTargeter();

  // Create the first window containing a Views::Textfield.
  CreateTextfield();
  aura::Window* window1 = textfield_widget_->GetNativeView();

  // Start touch editing, check that the handle is above the first window, and
  // end touch editing.
  StartTouchEditing();
  gfx::Point test_point = GetCursorHandleDragPoint();
  ui::MouseEvent test_event1(ui::ET_MOUSE_MOVED, test_point, test_point,
                             ui::EF_NONE, ui::EF_NONE);
  EXPECT_EQ(GetCursorHandleNativeView(),
            targeter->FindTargetForEvent(root, &test_event1));
  EndTouchEditing();

  // Create the second (empty) window over the first one.
  CreateWidget();
  aura::Window* window2 = widget_->GetNativeView();

  // Start touch editing (in the first window) and check that the handle is not
  // above the second window.
  StartTouchEditing();
  ui::MouseEvent test_event2(ui::ET_MOUSE_MOVED, test_point, test_point,
                             ui::EF_NONE, ui::EF_NONE);
  EXPECT_EQ(window2, targeter->FindTargetForEvent(root, &test_event2));

  // Move the first window to top and check that the handle is kept above the
  // first window.
  window1->GetRootWindow()->StackChildAtTop(window1);
  ui::MouseEvent test_event3(ui::ET_MOUSE_MOVED, test_point, test_point,
                             ui::EF_NONE, ui::EF_NONE);
  EXPECT_EQ(GetCursorHandleNativeView(),
            targeter->FindTargetForEvent(root, &test_event3));
}

// A simple implementation of TouchEditingMenuController that enables all
// available commands.
class TestTouchEditingMenuController : public TouchEditingMenuController {
 public:
  TestTouchEditingMenuController() {}
  virtual ~TestTouchEditingMenuController() {}

  // Overriden from TouchEditingMenuController.
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE {
    // Return true, since we want the menu to have all |kMenuCommandCount|
    // available commands.
    return true;
  }
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE {
    NOTREACHED();
  }
  virtual void OpenContextMenu() OVERRIDE {
    NOTREACHED();
  }
  virtual void OnMenuClosed(TouchEditingMenuView* menu) OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestTouchEditingMenuController);
};

// Tests if anchor rect for touch editing quick menu is adjusted correctly based
// on the distance of handles.
TEST_F(TouchSelectionControllerImplTest, QuickMenuAdjustsAnchorRect) {
  CreateWidget();
  aura::Window* window = widget_->GetNativeView();

  scoped_ptr<TestTouchEditingMenuController> quick_menu_controller(
      new TestTouchEditingMenuController());

  // Some arbitrary size for touch editing handle image.
  gfx::Size handle_image_size(10, 10);

  // Calculate the width of quick menu. In addition to |kMenuCommandCount|
  // commands, there is an item for ellipsis.
  int quick_menu_width = (kMenuCommandCount + 1) * kMenuButtonWidth +
                         kMenuCommandCount;

  // Set anchor rect's width a bit smaller than the quick menu width plus handle
  // image width and check that anchor rect's height is adjusted.
  gfx::Rect anchor_rect(
      0, 0, quick_menu_width + handle_image_size.width() - 10, 20);
  TouchEditingMenuView* quick_menu(TouchEditingMenuView::Create(
      quick_menu_controller.get(), anchor_rect, handle_image_size, window));
  anchor_rect.Inset(0, 0, 0, -handle_image_size.height());
  EXPECT_EQ(anchor_rect.ToString(), quick_menu->GetAnchorRect().ToString());

  // Set anchor rect's width a bit greater than the quick menu width plus handle
  // image width and check that anchor rect's height is not adjusted.
  anchor_rect =
      gfx::Rect(0, 0, quick_menu_width + handle_image_size.width() + 10, 20);
  quick_menu = TouchEditingMenuView::Create(
      quick_menu_controller.get(), anchor_rect, handle_image_size, window);
  EXPECT_EQ(anchor_rect.ToString(), quick_menu->GetAnchorRect().ToString());

  // Close the widget, hence quick menus, before quick menu controller goes out
  // of scope.
  widget_->CloseNow();
  widget_ = NULL;
}

TEST_F(TouchSelectionControllerImplTest, MouseEventDeactivatesTouchSelection) {
  CreateTextfield();
  EXPECT_FALSE(GetSelectionController());

  aura::test::EventGenerator generator(
      textfield_widget_->GetNativeView()->GetRootWindow());

  generator.set_current_location(gfx::Point(5, 5));
  RunPendingMessages();

  // Start touch editing; then move mouse over the textfield and ensure it
  // deactivates touch selection.
  StartTouchEditing();
  EXPECT_TRUE(GetSelectionController());
  generator.MoveMouseTo(gfx::Point(5, 10));
  RunPendingMessages();
  EXPECT_FALSE(GetSelectionController());

  generator.MoveMouseTo(gfx::Point(5, 50));
  RunPendingMessages();

  // Start touch editing; then move mouse out of the textfield, but inside the
  // winow and ensure it deactivates touch selection.
  StartTouchEditing();
  EXPECT_TRUE(GetSelectionController());
  generator.MoveMouseTo(gfx::Point(5, 55));
  RunPendingMessages();
  EXPECT_FALSE(GetSelectionController());

  generator.MoveMouseTo(gfx::Point(5, 500));
  RunPendingMessages();

  // Start touch editing; then move mouse out of the textfield and window and
  // ensure it deactivates touch selection.
  StartTouchEditing();
  EXPECT_TRUE(GetSelectionController());
  generator.MoveMouseTo(5, 505);
  RunPendingMessages();
  EXPECT_FALSE(GetSelectionController());
}

TEST_F(TouchSelectionControllerImplTest, KeyEventDeactivatesTouchSelection) {
  CreateTextfield();
  EXPECT_FALSE(GetSelectionController());

  aura::test::EventGenerator generator(
      textfield_widget_->GetNativeView()->GetRootWindow());

  RunPendingMessages();

  // Start touch editing; then press a key and ensure it deactivates touch
  // selection.
  StartTouchEditing();
  EXPECT_TRUE(GetSelectionController());
  generator.PressKey(ui::VKEY_A, 0);
  RunPendingMessages();
  EXPECT_FALSE(GetSelectionController());
}

}  // namespace views
