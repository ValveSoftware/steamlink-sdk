// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/textfield/textfield.h"

#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/pickle.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ui_strings.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/render_text.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/controls/textfield/textfield_model.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/ime/mock_input_method.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "ui/events/linux/text_edit_key_bindings_delegate_auralinux.h"
#endif

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;
using base::WideToUTF16;

#define EXPECT_STR_EQ(ascii, utf16) EXPECT_EQ(ASCIIToUTF16(ascii), utf16)

namespace {

const base::char16 kHebrewLetterSamekh = 0x05E1;

// A Textfield wrapper to intercept OnKey[Pressed|Released]() ressults.
class TestTextfield : public views::Textfield {
 public:
  TestTextfield() : Textfield(), key_handled_(false), key_received_(false) {}

  virtual bool OnKeyPressed(const ui::KeyEvent& e) OVERRIDE {
    key_received_ = true;
    key_handled_ = views::Textfield::OnKeyPressed(e);
    return key_handled_;
  }

  virtual bool OnKeyReleased(const ui::KeyEvent& e) OVERRIDE {
    key_received_ = true;
    key_handled_ = views::Textfield::OnKeyReleased(e);
    return key_handled_;
  }

  bool key_handled() const { return key_handled_; }
  bool key_received() const { return key_received_; }

  void clear() { key_received_ = key_handled_ = false; }

 private:
  bool key_handled_;
  bool key_received_;

  DISALLOW_COPY_AND_ASSIGN(TestTextfield);
};

// Convenience to make constructing a GestureEvent simpler.
class GestureEventForTest : public ui::GestureEvent {
 public:
  GestureEventForTest(ui::EventType type, int x, int y, float delta_x,
                      float delta_y)
      : GestureEvent(type, x, y, 0, base::TimeDelta(),
                     ui::GestureEventDetails(type, delta_x, delta_y), 0) {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GestureEventForTest);
};

base::string16 GetClipboardText(ui::ClipboardType type) {
  base::string16 text;
  ui::Clipboard::GetForCurrentThread()->ReadText(type, &text);
  return text;
}

void SetClipboardText(ui::ClipboardType type, const std::string& text) {
  ui::ScopedClipboardWriter(ui::Clipboard::GetForCurrentThread(), type)
      .WriteText(ASCIIToUTF16(text));
}

}  // namespace

namespace views {

class TextfieldTest : public ViewsTestBase, public TextfieldController {
 public:
  TextfieldTest()
      : widget_(NULL),
        textfield_(NULL),
        model_(NULL),
        input_method_(NULL),
        on_before_user_action_(0),
        on_after_user_action_(0),
        copied_to_clipboard_(ui::CLIPBOARD_TYPE_LAST) {
  }

  // ::testing::Test:
  virtual void SetUp() {
    ViewsTestBase::SetUp();
  }

  virtual void TearDown() {
    if (widget_)
      widget_->Close();
    ViewsTestBase::TearDown();
  }

  ui::ClipboardType GetAndResetCopiedToClipboard() {
    ui::ClipboardType clipboard_type = copied_to_clipboard_;
    copied_to_clipboard_ = ui::CLIPBOARD_TYPE_LAST;
    return clipboard_type;
  }

  // TextfieldController:
  virtual void ContentsChanged(Textfield* sender,
                               const base::string16& new_contents) OVERRIDE {
    // Paste calls TextfieldController::ContentsChanged() explicitly even if the
    // paste action did not change the content. So |new_contents| may match
    // |last_contents_|. For more info, see http://crbug.com/79002
    last_contents_ = new_contents;
  }

  virtual void OnBeforeUserAction(Textfield* sender) OVERRIDE {
    ++on_before_user_action_;
  }

  virtual void OnAfterUserAction(Textfield* sender) OVERRIDE {
    ++on_after_user_action_;
  }

  virtual void OnAfterCutOrCopy(ui::ClipboardType clipboard_type) OVERRIDE {
    copied_to_clipboard_ = clipboard_type;
  }

  void InitTextfield() {
    InitTextfields(1);
  }

  void InitTextfields(int count) {
    ASSERT_FALSE(textfield_);
    textfield_ = new TestTextfield();
    textfield_->set_controller(this);
    widget_ = new Widget();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(100, 100, 100, 100);
    widget_->Init(params);
    View* container = new View();
    widget_->SetContentsView(container);
    container->AddChildView(textfield_);
    textfield_->SetBoundsRect(params.bounds);
    textfield_->set_id(1);
    test_api_.reset(new TextfieldTestApi(textfield_));

    for (int i = 1; i < count; i++) {
      Textfield* textfield = new Textfield();
      container->AddChildView(textfield);
      textfield->set_id(i + 1);
    }

    model_ = test_api_->model();
    model_->ClearEditHistory();

    input_method_ = new MockInputMethod();
    widget_->ReplaceInputMethod(input_method_);

    // Activate the widget and focus the textfield for input handling.
    widget_->Activate();
    textfield_->RequestFocus();
  }

  ui::MenuModel* GetContextMenuModel() {
    test_api_->UpdateContextMenu();
    return test_api_->context_menu_contents();
  }

 protected:
  void SendKeyEvent(ui::KeyboardCode key_code,
                    bool alt,
                    bool shift,
                    bool control,
                    bool caps_lock) {
    int flags = (alt ? ui::EF_ALT_DOWN : 0) |
                (shift ? ui::EF_SHIFT_DOWN : 0) |
                (control ? ui::EF_CONTROL_DOWN : 0) |
                (caps_lock ? ui::EF_CAPS_LOCK_DOWN : 0);
    ui::KeyEvent event(ui::ET_KEY_PRESSED, key_code, flags, false);
    input_method_->DispatchKeyEvent(event);
  }

  void SendKeyEvent(ui::KeyboardCode key_code, bool shift, bool control) {
    SendKeyEvent(key_code, false, shift, control, false);
  }

  void SendKeyEvent(ui::KeyboardCode key_code) {
    SendKeyEvent(key_code, false, false);
  }

  void SendKeyEvent(base::char16 ch) {
    if (ch < 0x80) {
      ui::KeyboardCode code =
          ch == ' ' ? ui::VKEY_SPACE :
          static_cast<ui::KeyboardCode>(ui::VKEY_A + ch - 'a');
      SendKeyEvent(code);
    } else {
      ui::KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_UNKNOWN, 0, false);
      event.set_character(ch);
      input_method_->DispatchKeyEvent(event);
    }
  }

  View* GetFocusedView() {
    return widget_->GetFocusManager()->GetFocusedView();
  }

  int GetCursorPositionX(int cursor_pos) {
    return test_api_->GetRenderText()->GetCursorBounds(
        gfx::SelectionModel(cursor_pos, gfx::CURSOR_FORWARD), false).x();
  }

  // Get the current cursor bounds.
  gfx::Rect GetCursorBounds() {
    return test_api_->GetRenderText()->GetUpdatedCursorBounds();
  }

  // Get the cursor bounds of |sel|.
  gfx::Rect GetCursorBounds(const gfx::SelectionModel& sel) {
    return test_api_->GetRenderText()->GetCursorBounds(sel, true);
  }

  gfx::Rect GetDisplayRect() {
    return test_api_->GetRenderText()->display_rect();
  }

  // Mouse click on the point whose x-axis is |bound|'s x plus |x_offset| and
  // y-axis is in the middle of |bound|'s vertical range.
  void MouseClick(const gfx::Rect bound, int x_offset) {
    gfx::Point point(bound.x() + x_offset, bound.y() + bound.height() / 2);
    ui::MouseEvent click(ui::ET_MOUSE_PRESSED, point, point,
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    textfield_->OnMousePressed(click);
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, point, point,
                           ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    textfield_->OnMouseReleased(release);
  }

  // This is to avoid double/triple click.
  void NonClientMouseClick() {
    ui::MouseEvent click(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EF_LEFT_MOUSE_BUTTON | ui::EF_IS_NON_CLIENT,
                         ui::EF_LEFT_MOUSE_BUTTON);
    textfield_->OnMousePressed(click);
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                           ui::EF_LEFT_MOUSE_BUTTON | ui::EF_IS_NON_CLIENT,
                           ui::EF_LEFT_MOUSE_BUTTON);
    textfield_->OnMouseReleased(release);
  }

  void VerifyTextfieldContextMenuContents(bool textfield_has_selection,
                                          bool can_undo,
                                          ui::MenuModel* menu) {
    EXPECT_EQ(can_undo, menu->IsEnabledAt(0 /* UNDO */));
    EXPECT_TRUE(menu->IsEnabledAt(1 /* Separator */));
    EXPECT_EQ(textfield_has_selection, menu->IsEnabledAt(2 /* CUT */));
    EXPECT_EQ(textfield_has_selection, menu->IsEnabledAt(3 /* COPY */));
    EXPECT_NE(GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE).empty(),
              menu->IsEnabledAt(4 /* PASTE */));
    EXPECT_EQ(textfield_has_selection, menu->IsEnabledAt(5 /* DELETE */));
    EXPECT_TRUE(menu->IsEnabledAt(6 /* Separator */));
    EXPECT_TRUE(menu->IsEnabledAt(7 /* SELECT ALL */));
  }

  // We need widget to populate wrapper class.
  Widget* widget_;

  TestTextfield* textfield_;
  scoped_ptr<TextfieldTestApi> test_api_;
  TextfieldModel* model_;

  // The string from Controller::ContentsChanged callback.
  base::string16 last_contents_;

  // For testing input method related behaviors.
  MockInputMethod* input_method_;

  // Indicates how many times OnBeforeUserAction() is called.
  int on_before_user_action_;

  // Indicates how many times OnAfterUserAction() is called.
  int on_after_user_action_;

 private:
  ui::ClipboardType copied_to_clipboard_;

  DISALLOW_COPY_AND_ASSIGN(TextfieldTest);
};

TEST_F(TextfieldTest, ModelChangesTest) {
  InitTextfield();

  // TextfieldController::ContentsChanged() shouldn't be called when changing
  // text programmatically.
  last_contents_.clear();
  textfield_->SetText(ASCIIToUTF16("this is"));

  EXPECT_STR_EQ("this is", model_->text());
  EXPECT_STR_EQ("this is", textfield_->text());
  EXPECT_TRUE(last_contents_.empty());

  textfield_->AppendText(ASCIIToUTF16(" a test"));
  EXPECT_STR_EQ("this is a test", model_->text());
  EXPECT_STR_EQ("this is a test", textfield_->text());
  EXPECT_TRUE(last_contents_.empty());

  EXPECT_EQ(base::string16(), textfield_->GetSelectedText());
  textfield_->SelectAll(false);
  EXPECT_STR_EQ("this is a test", textfield_->GetSelectedText());
  EXPECT_TRUE(last_contents_.empty());
}

TEST_F(TextfieldTest, KeyTest) {
  InitTextfield();
  // Event flags:  key,    alt,   shift, ctrl,  caps-lock.
  SendKeyEvent(ui::VKEY_T, false, true,  false, false);
  SendKeyEvent(ui::VKEY_E, false, false, false, false);
  SendKeyEvent(ui::VKEY_X, false, true,  false, true);
  SendKeyEvent(ui::VKEY_T, false, false, false, true);
  SendKeyEvent(ui::VKEY_1, false, true,  false, false);
  SendKeyEvent(ui::VKEY_1, false, false, false, false);
  SendKeyEvent(ui::VKEY_1, false, true,  false, true);
  SendKeyEvent(ui::VKEY_1, false, false, false, true);
  EXPECT_STR_EQ("TexT!1!1", textfield_->text());
}

TEST_F(TextfieldTest, ControlAndSelectTest) {
  // Insert a test string in a textfield.
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("one two three"));
  SendKeyEvent(ui::VKEY_HOME,  false /* shift */, false /* control */);
  SendKeyEvent(ui::VKEY_RIGHT, true, false);
  SendKeyEvent(ui::VKEY_RIGHT, true, false);
  SendKeyEvent(ui::VKEY_RIGHT, true, false);

  EXPECT_STR_EQ("one", textfield_->GetSelectedText());

  // Test word select.
  SendKeyEvent(ui::VKEY_RIGHT, true, true);
  EXPECT_STR_EQ("one two", textfield_->GetSelectedText());
  SendKeyEvent(ui::VKEY_RIGHT, true, true);
  EXPECT_STR_EQ("one two three", textfield_->GetSelectedText());
  SendKeyEvent(ui::VKEY_LEFT, true, true);
  EXPECT_STR_EQ("one two ", textfield_->GetSelectedText());
  SendKeyEvent(ui::VKEY_LEFT, true, true);
  EXPECT_STR_EQ("one ", textfield_->GetSelectedText());

  // Replace the selected text.
  SendKeyEvent(ui::VKEY_Z, true, false);
  SendKeyEvent(ui::VKEY_E, true, false);
  SendKeyEvent(ui::VKEY_R, true, false);
  SendKeyEvent(ui::VKEY_O, true, false);
  SendKeyEvent(ui::VKEY_SPACE, false, false);
  EXPECT_STR_EQ("ZERO two three", textfield_->text());

  SendKeyEvent(ui::VKEY_END, true, false);
  EXPECT_STR_EQ("two three", textfield_->GetSelectedText());
  SendKeyEvent(ui::VKEY_HOME, true, false);
  EXPECT_STR_EQ("ZERO ", textfield_->GetSelectedText());
}

TEST_F(TextfieldTest, InsertionDeletionTest) {
  // Insert a test string in a textfield.
  InitTextfield();
  for (size_t i = 0; i < 10; i++)
    SendKeyEvent(static_cast<ui::KeyboardCode>(ui::VKEY_A + i));
  EXPECT_STR_EQ("abcdefghij", textfield_->text());

  // Test the delete and backspace keys.
  textfield_->SelectRange(gfx::Range(5));
  for (int i = 0; i < 3; i++)
    SendKeyEvent(ui::VKEY_BACK);
  EXPECT_STR_EQ("abfghij", textfield_->text());
  for (int i = 0; i < 3; i++)
    SendKeyEvent(ui::VKEY_DELETE);
  EXPECT_STR_EQ("abij", textfield_->text());

  // Select all and replace with "k".
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_K);
  EXPECT_STR_EQ("k", textfield_->text());

  // Delete the previous word from cursor.
  textfield_->SetText(ASCIIToUTF16("one two three four"));
  SendKeyEvent(ui::VKEY_END);
  SendKeyEvent(ui::VKEY_BACK, false, false, true, false);
  EXPECT_STR_EQ("one two three ", textfield_->text());

  // Delete to a line break on Linux and ChromeOS, to a word break on Windows.
  SendKeyEvent(ui::VKEY_LEFT, false, false, true, false);
  SendKeyEvent(ui::VKEY_BACK, false, true, true, false);
#if defined(OS_LINUX)
  EXPECT_STR_EQ("three ", textfield_->text());
#else
  EXPECT_STR_EQ("one three ", textfield_->text());
#endif

  // Delete the next word from cursor.
  textfield_->SetText(ASCIIToUTF16("one two three four"));
  SendKeyEvent(ui::VKEY_HOME);
  SendKeyEvent(ui::VKEY_DELETE, false, false, true, false);
  EXPECT_STR_EQ(" two three four", textfield_->text());

  // Delete to a line break on Linux and ChromeOS, to a word break on Windows.
  SendKeyEvent(ui::VKEY_RIGHT, false, false, true, false);
  SendKeyEvent(ui::VKEY_DELETE, false, true, true, false);
#if defined(OS_LINUX)
  EXPECT_STR_EQ(" two", textfield_->text());
#else
  EXPECT_STR_EQ(" two four", textfield_->text());
#endif
}

TEST_F(TextfieldTest, PasswordTest) {
  InitTextfield();
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, textfield_->GetTextInputType());
  EXPECT_TRUE(textfield_->enabled());
  EXPECT_TRUE(textfield_->IsFocusable());

  last_contents_.clear();
  textfield_->SetText(ASCIIToUTF16("password"));
  // Ensure text() and the callback returns the actual text instead of "*".
  EXPECT_STR_EQ("password", textfield_->text());
  EXPECT_TRUE(last_contents_.empty());
  model_->SelectAll(false);
  SetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE, "foo");

  // Cut and copy should be disabled.
  EXPECT_FALSE(textfield_->IsCommandIdEnabled(IDS_APP_CUT));
  textfield_->ExecuteCommand(IDS_APP_CUT, 0);
  SendKeyEvent(ui::VKEY_X, false, true);
  EXPECT_FALSE(textfield_->IsCommandIdEnabled(IDS_APP_COPY));
  textfield_->ExecuteCommand(IDS_APP_COPY, 0);
  SendKeyEvent(ui::VKEY_C, false, true);
  SendKeyEvent(ui::VKEY_INSERT, false, true);
  EXPECT_STR_EQ("foo", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_STR_EQ("password", textfield_->text());
  // [Shift]+[Delete] should just delete without copying text to the clipboard.
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_DELETE, true, false);

  // Paste should work normally.
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(IDS_APP_PASTE));
  textfield_->ExecuteCommand(IDS_APP_PASTE, 0);
  SendKeyEvent(ui::VKEY_V, false, true);
  SendKeyEvent(ui::VKEY_INSERT, true, false);
  EXPECT_STR_EQ("foo", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_STR_EQ("foofoofoo", textfield_->text());
}

TEST_F(TextfieldTest, TextInputType) {
  InitTextfield();

  // Defaults to TEXT
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, textfield_->GetTextInputType());

  // And can be set.
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_URL);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_URL, textfield_->GetTextInputType());
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, textfield_->GetTextInputType());

  // Readonly textfields have type NONE
  textfield_->SetReadOnly(true);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, textfield_->GetTextInputType());

  textfield_->SetReadOnly(false);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, textfield_->GetTextInputType());

  // As do disabled textfields
  textfield_->SetEnabled(false);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, textfield_->GetTextInputType());

  textfield_->SetEnabled(true);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, textfield_->GetTextInputType());
}

TEST_F(TextfieldTest, OnKeyPress) {
  InitTextfield();

  // Character keys are handled by the input method.
  SendKeyEvent(ui::VKEY_A);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_FALSE(textfield_->key_handled());
  textfield_->clear();

  // Arrow keys and home/end are handled by the textfield.
  SendKeyEvent(ui::VKEY_LEFT);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  textfield_->clear();

  SendKeyEvent(ui::VKEY_RIGHT);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  textfield_->clear();

  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  textfield_->clear();

  SendKeyEvent(ui::VKEY_END);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  textfield_->clear();

  // F24, up/down key won't be handled.
  SendKeyEvent(ui::VKEY_F24);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_FALSE(textfield_->key_handled());
  textfield_->clear();

  SendKeyEvent(ui::VKEY_UP);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_FALSE(textfield_->key_handled());
  textfield_->clear();

  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_FALSE(textfield_->key_handled());
  textfield_->clear();
}

// Tests that default key bindings are handled even with a delegate installed.
TEST_F(TextfieldTest, OnKeyPressBinding) {
  InitTextfield();

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // Install a TextEditKeyBindingsDelegateAuraLinux that does nothing.
  class TestDelegate : public ui::TextEditKeyBindingsDelegateAuraLinux {
   public:
    TestDelegate() {}
    virtual ~TestDelegate() {}

    virtual bool MatchEvent(
        const ui::Event& event,
        std::vector<ui::TextEditCommandAuraLinux>* commands) OVERRIDE {
      return false;
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(TestDelegate);
  };

  TestDelegate delegate;
  ui::SetTextEditKeyBindingsDelegate(&delegate);
#endif

  SendKeyEvent(ui::VKEY_A, false, false);
  EXPECT_STR_EQ("a", textfield_->text());
  textfield_->clear();

  // Undo/Redo command keys are handled by the textfield.
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  EXPECT_TRUE(textfield_->text().empty());
  textfield_->clear();

  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  EXPECT_STR_EQ("a", textfield_->text());
  textfield_->clear();

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  ui::SetTextEditKeyBindingsDelegate(NULL);
#endif
}

TEST_F(TextfieldTest, CursorMovement) {
  InitTextfield();

  // Test with trailing whitespace.
  textfield_->SetText(ASCIIToUTF16("one two hre "));

  // Send the cursor at the end.
  SendKeyEvent(ui::VKEY_END);

  // Ctrl+Left should move the cursor just before the last word.
  SendKeyEvent(ui::VKEY_LEFT, false, true);
  SendKeyEvent(ui::VKEY_T);
  EXPECT_STR_EQ("one two thre ", textfield_->text());
  EXPECT_STR_EQ("one two thre ", last_contents_);

  // Ctrl+Right should move the cursor to the end of the last word.
  SendKeyEvent(ui::VKEY_RIGHT, false, true);
  SendKeyEvent(ui::VKEY_E);
  EXPECT_STR_EQ("one two three ", textfield_->text());
  EXPECT_STR_EQ("one two three ", last_contents_);

  // Ctrl+Right again should move the cursor to the end.
  SendKeyEvent(ui::VKEY_RIGHT, false, true);
  SendKeyEvent(ui::VKEY_BACK);
  EXPECT_STR_EQ("one two three", textfield_->text());
  EXPECT_STR_EQ("one two three", last_contents_);

  // Test with leading whitespace.
  textfield_->SetText(ASCIIToUTF16(" ne two"));

  // Send the cursor at the beginning.
  SendKeyEvent(ui::VKEY_HOME);

  // Ctrl+Right, then Ctrl+Left should move the cursor to the beginning of the
  // first word.
  SendKeyEvent(ui::VKEY_RIGHT, false, true);
  SendKeyEvent(ui::VKEY_LEFT, false, true);
  SendKeyEvent(ui::VKEY_O);
  EXPECT_STR_EQ(" one two", textfield_->text());
  EXPECT_STR_EQ(" one two", last_contents_);

  // Ctrl+Left to move the cursor to the beginning of the first word.
  SendKeyEvent(ui::VKEY_LEFT, false, true);
  // Ctrl+Left again should move the cursor back to the very beginning.
  SendKeyEvent(ui::VKEY_LEFT, false, true);
  SendKeyEvent(ui::VKEY_DELETE);
  EXPECT_STR_EQ("one two", textfield_->text());
  EXPECT_STR_EQ("one two", last_contents_);
}

TEST_F(TextfieldTest, FocusTraversalTest) {
  InitTextfields(3);
  textfield_->RequestFocus();

  EXPECT_EQ(1, GetFocusedView()->id());
  widget_->GetFocusManager()->AdvanceFocus(false);
  EXPECT_EQ(2, GetFocusedView()->id());
  widget_->GetFocusManager()->AdvanceFocus(false);
  EXPECT_EQ(3, GetFocusedView()->id());
  // Cycle back to the first textfield.
  widget_->GetFocusManager()->AdvanceFocus(false);
  EXPECT_EQ(1, GetFocusedView()->id());

  widget_->GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(3, GetFocusedView()->id());
  widget_->GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(2, GetFocusedView()->id());
  widget_->GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(1, GetFocusedView()->id());
  // Cycle back to the last textfield.
  widget_->GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(3, GetFocusedView()->id());

  // Request focus should still work.
  textfield_->RequestFocus();
  EXPECT_EQ(1, GetFocusedView()->id());

  // Test if clicking on textfield view sets the focus.
  widget_->GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(3, GetFocusedView()->id());
  ui::MouseEvent click(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMousePressed(click);
  EXPECT_EQ(1, GetFocusedView()->id());
}

TEST_F(TextfieldTest, ContextMenuDisplayTest) {
  InitTextfield();
  EXPECT_TRUE(textfield_->context_menu_controller());
  textfield_->SetText(ASCIIToUTF16("hello world"));
  ui::Clipboard::GetForCurrentThread()->Clear(ui::CLIPBOARD_TYPE_COPY_PASTE);
  textfield_->ClearEditHistory();
  EXPECT_TRUE(GetContextMenuModel());
  VerifyTextfieldContextMenuContents(false, false, GetContextMenuModel());

  textfield_->SelectAll(false);
  VerifyTextfieldContextMenuContents(true, false, GetContextMenuModel());

  SendKeyEvent(ui::VKEY_T);
  VerifyTextfieldContextMenuContents(false, true, GetContextMenuModel());

  textfield_->SelectAll(false);
  VerifyTextfieldContextMenuContents(true, true, GetContextMenuModel());

  // Exercise the "paste enabled?" check in the verifier.
  SetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE, "Test");
  VerifyTextfieldContextMenuContents(true, true, GetContextMenuModel());
}

TEST_F(TextfieldTest, DoubleAndTripleClickTest) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));
  ui::MouseEvent click(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent double_click(
      ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_IS_DOUBLE_CLICK,
      ui::EF_LEFT_MOUSE_BUTTON);

  // Test for double click.
  textfield_->OnMousePressed(click);
  textfield_->OnMouseReleased(release);
  EXPECT_TRUE(textfield_->GetSelectedText().empty());
  textfield_->OnMousePressed(double_click);
  textfield_->OnMouseReleased(release);
  EXPECT_STR_EQ("hello", textfield_->GetSelectedText());

  // Test for triple click.
  textfield_->OnMousePressed(click);
  textfield_->OnMouseReleased(release);
  EXPECT_STR_EQ("hello world", textfield_->GetSelectedText());

  // Another click should reset back to double click.
  textfield_->OnMousePressed(click);
  textfield_->OnMouseReleased(release);
  EXPECT_STR_EQ("hello", textfield_->GetSelectedText());
}

TEST_F(TextfieldTest, DragToSelect) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));
  const int kStart = GetCursorPositionX(5);
  const int kEnd = 500;
  gfx::Point start_point(kStart, 0);
  gfx::Point end_point(kEnd, 0);
  ui::MouseEvent click_a(ui::ET_MOUSE_PRESSED, start_point, start_point,
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent click_b(ui::ET_MOUSE_PRESSED, end_point, end_point,
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent drag_left(ui::ET_MOUSE_DRAGGED, gfx::Point(), gfx::Point(),
                           ui::EF_LEFT_MOUSE_BUTTON, 0);
  ui::MouseEvent drag_right(ui::ET_MOUSE_DRAGGED, end_point, end_point,
                            ui::EF_LEFT_MOUSE_BUTTON, 0);
  ui::MouseEvent release(ui::ET_MOUSE_RELEASED, end_point, end_point,
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMousePressed(click_a);
  EXPECT_TRUE(textfield_->GetSelectedText().empty());
  // Check that dragging left selects the beginning of the string.
  textfield_->OnMouseDragged(drag_left);
  base::string16 text_left = textfield_->GetSelectedText();
  EXPECT_STR_EQ("hello", text_left);
  // Check that dragging right selects the rest of the string.
  textfield_->OnMouseDragged(drag_right);
  base::string16 text_right = textfield_->GetSelectedText();
  EXPECT_STR_EQ(" world", text_right);
  // Check that releasing in the same location does not alter the selection.
  textfield_->OnMouseReleased(release);
  EXPECT_EQ(text_right, textfield_->GetSelectedText());
  // Check that dragging from beyond the text length works too.
  textfield_->OnMousePressed(click_b);
  textfield_->OnMouseDragged(drag_left);
  textfield_->OnMouseReleased(release);
  EXPECT_EQ(textfield_->text(), textfield_->GetSelectedText());
}

#if defined(OS_WIN)
TEST_F(TextfieldTest, DragAndDrop_AcceptDrop) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));

  ui::OSExchangeData data;
  base::string16 string(ASCIIToUTF16("string "));
  data.SetString(string);
  int formats = 0;
  std::set<OSExchangeData::CustomFormat> custom_formats;

  // Ensure that disabled textfields do not accept drops.
  textfield_->SetEnabled(false);
  EXPECT_FALSE(textfield_->GetDropFormats(&formats, &custom_formats));
  EXPECT_EQ(0, formats);
  EXPECT_TRUE(custom_formats.empty());
  EXPECT_FALSE(textfield_->CanDrop(data));
  textfield_->SetEnabled(true);

  // Ensure that read-only textfields do not accept drops.
  textfield_->SetReadOnly(true);
  EXPECT_FALSE(textfield_->GetDropFormats(&formats, &custom_formats));
  EXPECT_EQ(0, formats);
  EXPECT_TRUE(custom_formats.empty());
  EXPECT_FALSE(textfield_->CanDrop(data));
  textfield_->SetReadOnly(false);

  // Ensure that enabled and editable textfields do accept drops.
  EXPECT_TRUE(textfield_->GetDropFormats(&formats, &custom_formats));
  EXPECT_EQ(ui::OSExchangeData::STRING, formats);
  EXPECT_TRUE(custom_formats.empty());
  EXPECT_TRUE(textfield_->CanDrop(data));
  gfx::Point drop_point(GetCursorPositionX(6), 0);
  ui::DropTargetEvent drop(data, drop_point, drop_point,
      ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE);
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE,
            textfield_->OnDragUpdated(drop));
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY, textfield_->OnPerformDrop(drop));
  EXPECT_STR_EQ("hello string world", textfield_->text());

  // Ensure that textfields do not accept non-OSExchangeData::STRING types.
  ui::OSExchangeData bad_data;
  bad_data.SetFilename(base::FilePath(FILE_PATH_LITERAL("x")));
  ui::OSExchangeData::CustomFormat fmt = ui::Clipboard::GetBitmapFormatType();
  bad_data.SetPickledData(fmt, Pickle());
  bad_data.SetFileContents(base::FilePath(L"x"), "x");
  bad_data.SetHtml(base::string16(ASCIIToUTF16("x")), GURL("x.org"));
  ui::OSExchangeData::DownloadFileInfo download(base::FilePath(), NULL);
  bad_data.SetDownloadFileInfo(download);
  EXPECT_FALSE(textfield_->CanDrop(bad_data));
}
#endif

TEST_F(TextfieldTest, DragAndDrop_InitiateDrag) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello string world"));

  // Ensure the textfield will provide selected text for drag data.
  base::string16 string;
  ui::OSExchangeData data;
  const gfx::Range kStringRange(6, 12);
  textfield_->SelectRange(kStringRange);
  const gfx::Point kStringPoint(GetCursorPositionX(9), 0);
  textfield_->WriteDragDataForView(NULL, kStringPoint, &data);
  EXPECT_TRUE(data.GetString(&string));
  EXPECT_EQ(textfield_->GetSelectedText(), string);

  // Ensure that disabled textfields do not support drag operations.
  textfield_->SetEnabled(false);
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE,
            textfield_->GetDragOperationsForView(NULL, kStringPoint));
  textfield_->SetEnabled(true);
  // Ensure that textfields without selections do not support drag operations.
  textfield_->ClearSelection();
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE,
            textfield_->GetDragOperationsForView(NULL, kStringPoint));
  textfield_->SelectRange(kStringRange);
  // Ensure that password textfields do not support drag operations.
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE,
            textfield_->GetDragOperationsForView(NULL, kStringPoint));
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_TEXT);
  // Ensure that textfields only initiate drag operations inside the selection.
  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, kStringPoint, kStringPoint,
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMousePressed(press_event);
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE,
            textfield_->GetDragOperationsForView(NULL, gfx::Point()));
  EXPECT_FALSE(textfield_->CanStartDragForView(NULL, gfx::Point(),
                                               gfx::Point()));
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY,
            textfield_->GetDragOperationsForView(NULL, kStringPoint));
  EXPECT_TRUE(textfield_->CanStartDragForView(NULL, kStringPoint,
                                              gfx::Point()));
  // Ensure that textfields support local moves.
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE | ui::DragDropTypes::DRAG_COPY,
      textfield_->GetDragOperationsForView(textfield_, kStringPoint));
}

TEST_F(TextfieldTest, DragAndDrop_ToTheRight) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));

  base::string16 string;
  ui::OSExchangeData data;
  int formats = 0;
  int operations = 0;
  std::set<OSExchangeData::CustomFormat> custom_formats;

  // Start dragging "ello".
  textfield_->SelectRange(gfx::Range(1, 5));
  gfx::Point point(GetCursorPositionX(3), 0);
  ui::MouseEvent click_a(ui::ET_MOUSE_PRESSED, point, point,
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMousePressed(click_a);
  EXPECT_TRUE(textfield_->CanStartDragForView(textfield_, click_a.location(),
                                              gfx::Point()));
  operations = textfield_->GetDragOperationsForView(textfield_,
                                                    click_a.location());
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE | ui::DragDropTypes::DRAG_COPY,
            operations);
  textfield_->WriteDragDataForView(NULL, click_a.location(), &data);
  EXPECT_TRUE(data.GetString(&string));
  EXPECT_EQ(textfield_->GetSelectedText(), string);
  EXPECT_TRUE(textfield_->GetDropFormats(&formats, &custom_formats));
  EXPECT_EQ(ui::OSExchangeData::STRING, formats);
  EXPECT_TRUE(custom_formats.empty());

  // Drop "ello" after "w".
  const gfx::Point kDropPoint(GetCursorPositionX(7), 0);
  EXPECT_TRUE(textfield_->CanDrop(data));
  ui::DropTargetEvent drop_a(data, kDropPoint, kDropPoint, operations);
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE, textfield_->OnDragUpdated(drop_a));
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE, textfield_->OnPerformDrop(drop_a));
  EXPECT_STR_EQ("h welloorld", textfield_->text());
  textfield_->OnDragDone();

  // Undo/Redo the drag&drop change.
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("hello world", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("hello world", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("h welloorld", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("h welloorld", textfield_->text());
}

TEST_F(TextfieldTest, DragAndDrop_ToTheLeft) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));

  base::string16 string;
  ui::OSExchangeData data;
  int formats = 0;
  int operations = 0;
  std::set<OSExchangeData::CustomFormat> custom_formats;

  // Start dragging " worl".
  textfield_->SelectRange(gfx::Range(5, 10));
  gfx::Point point(GetCursorPositionX(7), 0);
  ui::MouseEvent click_a(ui::ET_MOUSE_PRESSED, point, point,
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMousePressed(click_a);
  EXPECT_TRUE(textfield_->CanStartDragForView(textfield_, click_a.location(),
                                              gfx::Point()));
  operations = textfield_->GetDragOperationsForView(textfield_,
                                                    click_a.location());
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE | ui::DragDropTypes::DRAG_COPY,
            operations);
  textfield_->WriteDragDataForView(NULL, click_a.location(), &data);
  EXPECT_TRUE(data.GetString(&string));
  EXPECT_EQ(textfield_->GetSelectedText(), string);
  EXPECT_TRUE(textfield_->GetDropFormats(&formats, &custom_formats));
  EXPECT_EQ(ui::OSExchangeData::STRING, formats);
  EXPECT_TRUE(custom_formats.empty());

  // Drop " worl" after "h".
  EXPECT_TRUE(textfield_->CanDrop(data));
  gfx::Point drop_point(GetCursorPositionX(1), 0);
  ui::DropTargetEvent drop_a(data, drop_point, drop_point, operations);
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE, textfield_->OnDragUpdated(drop_a));
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE, textfield_->OnPerformDrop(drop_a));
  EXPECT_STR_EQ("h worlellod", textfield_->text());
  textfield_->OnDragDone();

  // Undo/Redo the drag&drop change.
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("hello world", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("hello world", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("h worlellod", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("h worlellod", textfield_->text());
}

TEST_F(TextfieldTest, DragAndDrop_Canceled) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));

  // Start dragging "worl".
  textfield_->SelectRange(gfx::Range(6, 10));
  gfx::Point point(GetCursorPositionX(8), 0);
  ui::MouseEvent click(ui::ET_MOUSE_PRESSED, point, point,
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMousePressed(click);
  ui::OSExchangeData data;
  textfield_->WriteDragDataForView(NULL, click.location(), &data);
  EXPECT_TRUE(textfield_->CanDrop(data));
  // Drag the text over somewhere valid, outside the current selection.
  gfx::Point drop_point(GetCursorPositionX(2), 0);
  ui::DropTargetEvent drop(data, drop_point, drop_point,
                           ui::DragDropTypes::DRAG_MOVE);
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE, textfield_->OnDragUpdated(drop));
  // "Cancel" the drag, via move and release over the selection, and OnDragDone.
  gfx::Point drag_point(GetCursorPositionX(9), 0);
  ui::MouseEvent drag(ui::ET_MOUSE_DRAGGED, drag_point, drag_point,
                      ui::EF_LEFT_MOUSE_BUTTON, 0);
  ui::MouseEvent release(ui::ET_MOUSE_RELEASED, drag_point, drag_point,
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMouseDragged(drag);
  textfield_->OnMouseReleased(release);
  textfield_->OnDragDone();
  EXPECT_EQ(ASCIIToUTF16("hello world"), textfield_->text());
}

TEST_F(TextfieldTest, ReadOnlyTest) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("read only"));
  textfield_->SetReadOnly(true);
  EXPECT_TRUE(textfield_->enabled());
  EXPECT_TRUE(textfield_->IsFocusable());

  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_EQ(0U, textfield_->GetCursorPosition());
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(9U, textfield_->GetCursorPosition());

  SendKeyEvent(ui::VKEY_LEFT, false, false);
  EXPECT_EQ(8U, textfield_->GetCursorPosition());
  SendKeyEvent(ui::VKEY_LEFT, false, true);
  EXPECT_EQ(5U, textfield_->GetCursorPosition());
  SendKeyEvent(ui::VKEY_LEFT, true, true);
  EXPECT_EQ(0U, textfield_->GetCursorPosition());
  EXPECT_STR_EQ("read ", textfield_->GetSelectedText());
  textfield_->SelectAll(false);
  EXPECT_STR_EQ("read only", textfield_->GetSelectedText());

  // Cut should be disabled.
  SetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE, "Test");
  EXPECT_FALSE(textfield_->IsCommandIdEnabled(IDS_APP_CUT));
  textfield_->ExecuteCommand(IDS_APP_CUT, 0);
  SendKeyEvent(ui::VKEY_X, false, true);
  SendKeyEvent(ui::VKEY_DELETE, true, false);
  EXPECT_STR_EQ("Test", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_STR_EQ("read only", textfield_->text());

  // Paste should be disabled.
  EXPECT_FALSE(textfield_->IsCommandIdEnabled(IDS_APP_PASTE));
  textfield_->ExecuteCommand(IDS_APP_PASTE, 0);
  SendKeyEvent(ui::VKEY_V, false, true);
  SendKeyEvent(ui::VKEY_INSERT, true, false);
  EXPECT_STR_EQ("read only", textfield_->text());

  // Copy should work normally.
  SetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE, "Test");
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(IDS_APP_COPY));
  textfield_->ExecuteCommand(IDS_APP_COPY, 0);
  EXPECT_STR_EQ("read only", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  SetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE, "Test");
  SendKeyEvent(ui::VKEY_C, false, true);
  EXPECT_STR_EQ("read only", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  SetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE, "Test");
  SendKeyEvent(ui::VKEY_INSERT, false, true);
  EXPECT_STR_EQ("read only", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));

  // SetText should work even in read only mode.
  textfield_->SetText(ASCIIToUTF16(" four five six "));
  EXPECT_STR_EQ(" four five six ", textfield_->text());

  textfield_->SelectAll(false);
  EXPECT_STR_EQ(" four five six ", textfield_->GetSelectedText());

  // Text field is unmodifiable and selection shouldn't change.
  SendKeyEvent(ui::VKEY_DELETE);
  EXPECT_STR_EQ(" four five six ", textfield_->GetSelectedText());
  SendKeyEvent(ui::VKEY_BACK);
  EXPECT_STR_EQ(" four five six ", textfield_->GetSelectedText());
  SendKeyEvent(ui::VKEY_T);
  EXPECT_STR_EQ(" four five six ", textfield_->GetSelectedText());
}

TEST_F(TextfieldTest, TextInputClientTest) {
  InitTextfield();
  ui::TextInputClient* client = textfield_->GetTextInputClient();
  EXPECT_TRUE(client);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, client->GetTextInputType());

  textfield_->SetText(ASCIIToUTF16("0123456789"));
  gfx::Range range;
  EXPECT_TRUE(client->GetTextRange(&range));
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(10U, range.end());

  EXPECT_TRUE(client->SetSelectionRange(gfx::Range(1, 4)));
  EXPECT_TRUE(client->GetSelectionRange(&range));
  EXPECT_EQ(gfx::Range(1, 4), range);

  base::string16 substring;
  EXPECT_TRUE(client->GetTextFromRange(range, &substring));
  EXPECT_STR_EQ("123", substring);

  EXPECT_TRUE(client->DeleteRange(range));
  EXPECT_STR_EQ("0456789", textfield_->text());

  ui::CompositionText composition;
  composition.text = UTF8ToUTF16("321");
  // Set composition through input method.
  input_method_->Clear();
  input_method_->SetCompositionTextForNextKey(composition);
  textfield_->clear();

  on_before_user_action_ = on_after_user_action_ = 0;
  SendKeyEvent(ui::VKEY_A);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_FALSE(textfield_->key_handled());
  EXPECT_TRUE(client->HasCompositionText());
  EXPECT_TRUE(client->GetCompositionTextRange(&range));
  EXPECT_STR_EQ("0321456789", textfield_->text());
  EXPECT_EQ(gfx::Range(1, 4), range);
  EXPECT_EQ(1, on_before_user_action_);
  EXPECT_EQ(1, on_after_user_action_);

  input_method_->SetResultTextForNextKey(UTF8ToUTF16("123"));
  on_before_user_action_ = on_after_user_action_ = 0;
  textfield_->clear();
  SendKeyEvent(ui::VKEY_A);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_FALSE(textfield_->key_handled());
  EXPECT_FALSE(client->HasCompositionText());
  EXPECT_FALSE(input_method_->cancel_composition_called());
  EXPECT_STR_EQ("0123456789", textfield_->text());
  EXPECT_EQ(1, on_before_user_action_);
  EXPECT_EQ(1, on_after_user_action_);

  input_method_->Clear();
  input_method_->SetCompositionTextForNextKey(composition);
  textfield_->clear();
  SendKeyEvent(ui::VKEY_A);
  EXPECT_TRUE(client->HasCompositionText());
  EXPECT_STR_EQ("0123321456789", textfield_->text());

  on_before_user_action_ = on_after_user_action_ = 0;
  textfield_->clear();
  SendKeyEvent(ui::VKEY_RIGHT);
  EXPECT_FALSE(client->HasCompositionText());
  EXPECT_TRUE(input_method_->cancel_composition_called());
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  EXPECT_STR_EQ("0123321456789", textfield_->text());
  EXPECT_EQ(8U, textfield_->GetCursorPosition());
  EXPECT_EQ(1, on_before_user_action_);
  EXPECT_EQ(1, on_after_user_action_);

  textfield_->clear();
  textfield_->SetText(ASCIIToUTF16("0123456789"));
  EXPECT_TRUE(client->SetSelectionRange(gfx::Range(5, 5)));
  client->ExtendSelectionAndDelete(4, 2);
  EXPECT_STR_EQ("0789", textfield_->text());

  // On{Before,After}UserAction should be called by whatever user action
  // triggers clearing or setting a selection if appropriate.
  on_before_user_action_ = on_after_user_action_ = 0;
  textfield_->clear();
  textfield_->ClearSelection();
  textfield_->SelectAll(false);
  EXPECT_EQ(0, on_before_user_action_);
  EXPECT_EQ(0, on_after_user_action_);

  input_method_->Clear();
  textfield_->SetReadOnly(true);
  EXPECT_TRUE(input_method_->text_input_type_changed());
  EXPECT_FALSE(textfield_->GetTextInputClient());

  textfield_->SetReadOnly(false);
  input_method_->Clear();
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_TRUE(input_method_->text_input_type_changed());
  EXPECT_TRUE(textfield_->GetTextInputClient());
}

TEST_F(TextfieldTest, UndoRedoTest) {
  InitTextfield();
  SendKeyEvent(ui::VKEY_A);
  EXPECT_STR_EQ("a", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("a", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("a", textfield_->text());

  // AppendText
  textfield_->AppendText(ASCIIToUTF16("b"));
  last_contents_.clear();  // AppendText doesn't call ContentsChanged.
  EXPECT_STR_EQ("ab", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("a", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("ab", textfield_->text());

  // SetText
  SendKeyEvent(ui::VKEY_C);
  // Undo'ing append moves the cursor to the end for now.
  // A no-op SetText won't add a new edit; see TextfieldModel::SetText.
  EXPECT_STR_EQ("abc", textfield_->text());
  textfield_->SetText(ASCIIToUTF16("abc"));
  EXPECT_STR_EQ("abc", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("ab", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("abc", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("abc", textfield_->text());
  textfield_->SetText(ASCIIToUTF16("123"));
  textfield_->SetText(ASCIIToUTF16("123"));
  EXPECT_STR_EQ("123", textfield_->text());
  SendKeyEvent(ui::VKEY_END, false, false);
  SendKeyEvent(ui::VKEY_4, false, false);
  EXPECT_STR_EQ("1234", textfield_->text());
  last_contents_.clear();
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("123", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  // the insert edit "c" and set edit "123" are merged to single edit,
  // so text becomes "ab" after undo.
  EXPECT_STR_EQ("ab", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("a", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("ab", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("123", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("1234", textfield_->text());

  // Undoing to the same text shouldn't call ContentsChanged.
  SendKeyEvent(ui::VKEY_A, false, true);  // select all
  SendKeyEvent(ui::VKEY_A);
  EXPECT_STR_EQ("a", textfield_->text());
  SendKeyEvent(ui::VKEY_B);
  SendKeyEvent(ui::VKEY_C);
  EXPECT_STR_EQ("abc", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("1234", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("abc", textfield_->text());

  // Delete/Backspace
  SendKeyEvent(ui::VKEY_BACK);
  EXPECT_STR_EQ("ab", textfield_->text());
  SendKeyEvent(ui::VKEY_HOME);
  SendKeyEvent(ui::VKEY_DELETE);
  EXPECT_STR_EQ("b", textfield_->text());
  SendKeyEvent(ui::VKEY_A, false, true);
  SendKeyEvent(ui::VKEY_DELETE);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("b", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("ab", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("abc", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("ab", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("b", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("", textfield_->text());
}

TEST_F(TextfieldTest, CutCopyPaste) {
  InitTextfield();

  // Ensure IDS_APP_CUT cuts.
  textfield_->SetText(ASCIIToUTF16("123"));
  textfield_->SelectAll(false);
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(IDS_APP_CUT));
  textfield_->ExecuteCommand(IDS_APP_CUT, 0);
  EXPECT_STR_EQ("123", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_STR_EQ("", textfield_->text());
  EXPECT_EQ(ui::CLIPBOARD_TYPE_COPY_PASTE, GetAndResetCopiedToClipboard());

  // Ensure [Ctrl]+[x] cuts and [Ctrl]+[Alt][x] does nothing.
  textfield_->SetText(ASCIIToUTF16("456"));
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_X, true, false, true, false);
  EXPECT_STR_EQ("123", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_STR_EQ("456", textfield_->text());
  EXPECT_EQ(ui::CLIPBOARD_TYPE_LAST, GetAndResetCopiedToClipboard());
  SendKeyEvent(ui::VKEY_X, false, true);
  EXPECT_STR_EQ("456", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_STR_EQ("", textfield_->text());
  EXPECT_EQ(ui::CLIPBOARD_TYPE_COPY_PASTE, GetAndResetCopiedToClipboard());

  // Ensure [Shift]+[Delete] cuts.
  textfield_->SetText(ASCIIToUTF16("123"));
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_DELETE, true, false);
  EXPECT_STR_EQ("123", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_STR_EQ("", textfield_->text());
  EXPECT_EQ(ui::CLIPBOARD_TYPE_COPY_PASTE, GetAndResetCopiedToClipboard());

  // Ensure IDS_APP_COPY copies.
  textfield_->SetText(ASCIIToUTF16("789"));
  textfield_->SelectAll(false);
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(IDS_APP_COPY));
  textfield_->ExecuteCommand(IDS_APP_COPY, 0);
  EXPECT_STR_EQ("789", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_COPY_PASTE, GetAndResetCopiedToClipboard());

  // Ensure [Ctrl]+[c] copies and [Ctrl]+[Alt][c] does nothing.
  textfield_->SetText(ASCIIToUTF16("012"));
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_C, true, false, true, false);
  EXPECT_STR_EQ("789", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_LAST, GetAndResetCopiedToClipboard());
  SendKeyEvent(ui::VKEY_C, false, true);
  EXPECT_STR_EQ("012", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_COPY_PASTE, GetAndResetCopiedToClipboard());

  // Ensure [Ctrl]+[Insert] copies.
  textfield_->SetText(ASCIIToUTF16("345"));
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_INSERT, false, true);
  EXPECT_STR_EQ("345", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_STR_EQ("345", textfield_->text());
  EXPECT_EQ(ui::CLIPBOARD_TYPE_COPY_PASTE, GetAndResetCopiedToClipboard());

  // Ensure IDS_APP_PASTE, [Ctrl]+[V], and [Shift]+[Insert] pastes;
  // also ensure that [Ctrl]+[Alt]+[V] does nothing.
  SetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE, "abc");
  textfield_->SetText(base::string16());
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(IDS_APP_PASTE));
  textfield_->ExecuteCommand(IDS_APP_PASTE, 0);
  EXPECT_STR_EQ("abc", textfield_->text());
  SendKeyEvent(ui::VKEY_V, false, true);
  EXPECT_STR_EQ("abcabc", textfield_->text());
  SendKeyEvent(ui::VKEY_INSERT, true, false);
  EXPECT_STR_EQ("abcabcabc", textfield_->text());
  SendKeyEvent(ui::VKEY_V, true, false, true, false);
  EXPECT_STR_EQ("abcabcabc", textfield_->text());

  // Ensure [Ctrl]+[Shift]+[Insert] is a no-op.
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_INSERT, true, true);
  EXPECT_STR_EQ("abc", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_STR_EQ("abcabcabc", textfield_->text());
  EXPECT_EQ(ui::CLIPBOARD_TYPE_LAST, GetAndResetCopiedToClipboard());
}

TEST_F(TextfieldTest, OvertypeMode) {
  InitTextfield();
  // Overtype mode should be disabled (no-op [Insert]).
  textfield_->SetText(ASCIIToUTF16("2"));
  SendKeyEvent(ui::VKEY_HOME);
  SendKeyEvent(ui::VKEY_INSERT);
  SendKeyEvent(ui::VKEY_1, false, false);
  EXPECT_STR_EQ("12", textfield_->text());
}

TEST_F(TextfieldTest, TextCursorDisplayTest) {
  InitTextfield();
  // LTR-RTL string in LTR context.
  SendKeyEvent('a');
  EXPECT_STR_EQ("a", textfield_->text());
  int x = GetCursorBounds().x();
  int prev_x = x;

  SendKeyEvent('b');
  EXPECT_STR_EQ("ab", textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_LT(prev_x, x);
  prev_x = x;

  SendKeyEvent(0x05E1);
  EXPECT_EQ(WideToUTF16(L"ab\x05E1"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_EQ(prev_x, x);

  SendKeyEvent(0x05E2);
  EXPECT_EQ(WideToUTF16(L"ab\x05E1\x5E2"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_EQ(prev_x, x);

  // Clear text.
  SendKeyEvent(ui::VKEY_A, false, true);
  SendKeyEvent('\n');

  // RTL-LTR string in LTR context.
  SendKeyEvent(0x05E1);
  EXPECT_EQ(WideToUTF16(L"\x05E1"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_EQ(GetDisplayRect().x(), x);
  prev_x = x;

  SendKeyEvent(0x05E2);
  EXPECT_EQ(WideToUTF16(L"\x05E1\x05E2"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_EQ(prev_x, x);

  SendKeyEvent('a');
  EXPECT_EQ(WideToUTF16(L"\x05E1\x5E2" L"a"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_LT(prev_x, x);
  prev_x = x;

  SendKeyEvent('b');
  EXPECT_EQ(WideToUTF16(L"\x05E1\x5E2" L"ab"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_LT(prev_x, x);
}

TEST_F(TextfieldTest, TextCursorDisplayInRTLTest) {
  std::string locale = l10n_util::GetApplicationLocale("");
  base::i18n::SetICUDefaultLocale("he");

  InitTextfield();
  // LTR-RTL string in RTL context.
  SendKeyEvent('a');
  EXPECT_STR_EQ("a", textfield_->text());
  int x = GetCursorBounds().x();
  EXPECT_EQ(GetDisplayRect().right() - 1, x);
  int prev_x = x;

  SendKeyEvent('b');
  EXPECT_STR_EQ("ab", textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_EQ(prev_x, x);

  SendKeyEvent(0x05E1);
  EXPECT_EQ(WideToUTF16(L"ab\x05E1"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_GT(prev_x, x);
  prev_x = x;

  SendKeyEvent(0x05E2);
  EXPECT_EQ(WideToUTF16(L"ab\x05E1\x5E2"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_GT(prev_x, x);

  SendKeyEvent(ui::VKEY_A, false, true);
  SendKeyEvent('\n');

  // RTL-LTR string in RTL context.
  SendKeyEvent(0x05E1);
  EXPECT_EQ(WideToUTF16(L"\x05E1"), textfield_->text());
  x = GetCursorBounds().x();
  prev_x = x;

  SendKeyEvent(0x05E2);
  EXPECT_EQ(WideToUTF16(L"\x05E1\x05E2"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_GT(prev_x, x);
  prev_x = x;

  SendKeyEvent('a');
  EXPECT_EQ(WideToUTF16(L"\x05E1\x5E2" L"a"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_EQ(prev_x, x);
  prev_x = x;

  SendKeyEvent('b');
  EXPECT_EQ(WideToUTF16(L"\x05E1\x5E2" L"ab"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_EQ(prev_x, x);

  // Reset locale.
  base::i18n::SetICUDefaultLocale(locale);
}

TEST_F(TextfieldTest, HitInsideTextAreaTest) {
  InitTextfield();
  textfield_->SetText(WideToUTF16(L"ab\x05E1\x5E2"));
  std::vector<gfx::Rect> cursor_bounds;

  // Save each cursor bound.
  gfx::SelectionModel sel(0, gfx::CURSOR_FORWARD);
  cursor_bounds.push_back(GetCursorBounds(sel));

  sel = gfx::SelectionModel(1, gfx::CURSOR_BACKWARD);
  gfx::Rect bound = GetCursorBounds(sel);
  sel = gfx::SelectionModel(1, gfx::CURSOR_FORWARD);
  EXPECT_EQ(bound.x(), GetCursorBounds(sel).x());
  cursor_bounds.push_back(bound);

  // Check that a cursor at the end of the Latin portion of the text is at the
  // same position as a cursor placed at the end of the RTL Hebrew portion.
  sel = gfx::SelectionModel(2, gfx::CURSOR_BACKWARD);
  bound = GetCursorBounds(sel);
  sel = gfx::SelectionModel(4, gfx::CURSOR_BACKWARD);
  EXPECT_EQ(bound.x(), GetCursorBounds(sel).x());
  cursor_bounds.push_back(bound);

  sel = gfx::SelectionModel(3, gfx::CURSOR_BACKWARD);
  bound = GetCursorBounds(sel);
  sel = gfx::SelectionModel(3, gfx::CURSOR_FORWARD);
  EXPECT_EQ(bound.x(), GetCursorBounds(sel).x());
  cursor_bounds.push_back(bound);

  sel = gfx::SelectionModel(2, gfx::CURSOR_FORWARD);
  bound = GetCursorBounds(sel);
  sel = gfx::SelectionModel(4, gfx::CURSOR_FORWARD);
  EXPECT_EQ(bound.x(), GetCursorBounds(sel).x());
  cursor_bounds.push_back(bound);

  // Expected cursor position when clicking left and right of each character.
  size_t cursor_pos_expected[] = {0, 1, 1, 2, 4, 3, 3, 2};

  int index = 0;
  for (int i = 0; i < static_cast<int>(cursor_bounds.size() - 1); ++i) {
    int half_width = (cursor_bounds[i + 1].x() - cursor_bounds[i].x()) / 2;
    MouseClick(cursor_bounds[i], half_width / 2);
    EXPECT_EQ(cursor_pos_expected[index++], textfield_->GetCursorPosition());

    // To avoid trigger double click. Not using sleep() since it takes longer
    // for the test to run if using sleep().
    NonClientMouseClick();

    MouseClick(cursor_bounds[i + 1], - (half_width / 2));
    EXPECT_EQ(cursor_pos_expected[index++], textfield_->GetCursorPosition());

    NonClientMouseClick();
  }
}

TEST_F(TextfieldTest, HitOutsideTextAreaTest) {
  InitTextfield();

  // LTR-RTL string in LTR context.
  textfield_->SetText(WideToUTF16(L"ab\x05E1\x5E2"));

  SendKeyEvent(ui::VKEY_HOME);
  gfx::Rect bound = GetCursorBounds();
  MouseClick(bound, -10);
  EXPECT_EQ(bound, GetCursorBounds());

  SendKeyEvent(ui::VKEY_END);
  bound = GetCursorBounds();
  MouseClick(bound, 10);
  EXPECT_EQ(bound, GetCursorBounds());

  NonClientMouseClick();

  // RTL-LTR string in LTR context.
  textfield_->SetText(WideToUTF16(L"\x05E1\x5E2" L"ab"));

  SendKeyEvent(ui::VKEY_HOME);
  bound = GetCursorBounds();
  MouseClick(bound, 10);
  EXPECT_EQ(bound, GetCursorBounds());

  SendKeyEvent(ui::VKEY_END);
  bound = GetCursorBounds();
  MouseClick(bound, -10);
  EXPECT_EQ(bound, GetCursorBounds());
}

TEST_F(TextfieldTest, HitOutsideTextAreaInRTLTest) {
  std::string locale = l10n_util::GetApplicationLocale("");
  base::i18n::SetICUDefaultLocale("he");

  InitTextfield();

  // RTL-LTR string in RTL context.
  textfield_->SetText(WideToUTF16(L"\x05E1\x5E2" L"ab"));
  SendKeyEvent(ui::VKEY_HOME);
  gfx::Rect bound = GetCursorBounds();
  MouseClick(bound, 10);
  EXPECT_EQ(bound, GetCursorBounds());

  SendKeyEvent(ui::VKEY_END);
  bound = GetCursorBounds();
  MouseClick(bound, -10);
  EXPECT_EQ(bound, GetCursorBounds());

  NonClientMouseClick();

  // LTR-RTL string in RTL context.
  textfield_->SetText(WideToUTF16(L"ab\x05E1\x5E2"));
  SendKeyEvent(ui::VKEY_HOME);
  bound = GetCursorBounds();
  MouseClick(bound, -10);
  EXPECT_EQ(bound, GetCursorBounds());

  SendKeyEvent(ui::VKEY_END);
  bound = GetCursorBounds();
  MouseClick(bound, 10);
  EXPECT_EQ(bound, GetCursorBounds());

  // Reset locale.
  base::i18n::SetICUDefaultLocale(locale);
}

TEST_F(TextfieldTest, OverflowTest) {
  InitTextfield();

  base::string16 str;
  for (int i = 0; i < 500; ++i)
    SendKeyEvent('a');
  SendKeyEvent(kHebrewLetterSamekh);
  EXPECT_TRUE(GetDisplayRect().Contains(GetCursorBounds()));

  // Test mouse pointing.
  MouseClick(GetCursorBounds(), -1);
  EXPECT_EQ(500U, textfield_->GetCursorPosition());

  // Clear text.
  SendKeyEvent(ui::VKEY_A, false, true);
  SendKeyEvent('\n');

  for (int i = 0; i < 500; ++i)
    SendKeyEvent(kHebrewLetterSamekh);
  SendKeyEvent('a');
  EXPECT_TRUE(GetDisplayRect().Contains(GetCursorBounds()));

  MouseClick(GetCursorBounds(), -1);
  EXPECT_EQ(501U, textfield_->GetCursorPosition());
}

TEST_F(TextfieldTest, OverflowInRTLTest) {
  std::string locale = l10n_util::GetApplicationLocale("");
  base::i18n::SetICUDefaultLocale("he");

  InitTextfield();

  base::string16 str;
  for (int i = 0; i < 500; ++i)
    SendKeyEvent('a');
  SendKeyEvent(kHebrewLetterSamekh);
  EXPECT_TRUE(GetDisplayRect().Contains(GetCursorBounds()));

  MouseClick(GetCursorBounds(), 1);
  EXPECT_EQ(501U, textfield_->GetCursorPosition());

  // Clear text.
  SendKeyEvent(ui::VKEY_A, false, true);
  SendKeyEvent('\n');

  for (int i = 0; i < 500; ++i)
    SendKeyEvent(kHebrewLetterSamekh);
  SendKeyEvent('a');
  EXPECT_TRUE(GetDisplayRect().Contains(GetCursorBounds()));

  MouseClick(GetCursorBounds(), 1);
  EXPECT_EQ(500U, textfield_->GetCursorPosition());

  // Reset locale.
  base::i18n::SetICUDefaultLocale(locale);
}

TEST_F(TextfieldTest, GetCompositionCharacterBoundsTest) {
  InitTextfield();

  base::string16 str;
  const uint32 char_count = 10UL;
  ui::CompositionText composition;
  composition.text = UTF8ToUTF16("0123456789");
  ui::TextInputClient* client = textfield_->GetTextInputClient();

  // Return false if there is no composition text.
  gfx::Rect rect;
  EXPECT_FALSE(client->GetCompositionCharacterBounds(0, &rect));

  // Get each character boundary by cursor.
  gfx::Rect char_rect_in_screen_coord[char_count];
  gfx::Rect prev_cursor = GetCursorBounds();
  for (uint32 i = 0; i < char_count; ++i) {
    composition.selection = gfx::Range(0, i+1);
    client->SetCompositionText(composition);
    EXPECT_TRUE(client->HasCompositionText()) << " i=" << i;
    gfx::Rect cursor_bounds = GetCursorBounds();
    gfx::Point top_left(prev_cursor.x(), prev_cursor.y());
    gfx::Point bottom_right(cursor_bounds.x(), prev_cursor.bottom());
    views::View::ConvertPointToScreen(textfield_, &top_left);
    views::View::ConvertPointToScreen(textfield_, &bottom_right);
    char_rect_in_screen_coord[i].set_origin(top_left);
    char_rect_in_screen_coord[i].set_width(bottom_right.x() - top_left.x());
    char_rect_in_screen_coord[i].set_height(bottom_right.y() - top_left.y());
    prev_cursor = cursor_bounds;
  }

  for (uint32 i = 0; i < char_count; ++i) {
    gfx::Rect actual_rect;
    EXPECT_TRUE(client->GetCompositionCharacterBounds(i, &actual_rect))
        << " i=" << i;
    EXPECT_EQ(char_rect_in_screen_coord[i], actual_rect) << " i=" << i;
  }

  // Return false if the index is out of range.
  EXPECT_FALSE(client->GetCompositionCharacterBounds(char_count, &rect));
  EXPECT_FALSE(client->GetCompositionCharacterBounds(char_count + 1, &rect));
  EXPECT_FALSE(client->GetCompositionCharacterBounds(char_count + 100, &rect));
}

TEST_F(TextfieldTest, GetCompositionCharacterBounds_ComplexText) {
  InitTextfield();

  const base::char16 kUtf16Chars[] = {
    // U+0020 SPACE
    0x0020,
    // U+1F408 (CAT) as surrogate pair
    0xd83d, 0xdc08,
    // U+5642 as Ideographic Variation Sequences
    0x5642, 0xDB40, 0xDD00,
    // U+260E (BLACK TELEPHONE) as Emoji Variation Sequences
    0x260E, 0xFE0F,
    // U+0020 SPACE
    0x0020,
  };
  const size_t kUtf16CharsCount = arraysize(kUtf16Chars);

  ui::CompositionText composition;
  composition.text.assign(kUtf16Chars, kUtf16Chars + kUtf16CharsCount);
  ui::TextInputClient* client = textfield_->GetTextInputClient();
  client->SetCompositionText(composition);

  // Make sure GetCompositionCharacterBounds never fails for index.
  gfx::Rect rects[kUtf16CharsCount];
  gfx::Rect prev_cursor = GetCursorBounds();
  for (uint32 i = 0; i < kUtf16CharsCount; ++i)
    EXPECT_TRUE(client->GetCompositionCharacterBounds(i, &rects[i]));

  // Here we might expect the following results but it actually depends on how
  // Uniscribe or HarfBuzz treats them with given font.
  // - rects[1] == rects[2]
  // - rects[3] == rects[4] == rects[5]
  // - rects[6] == rects[7]
}

// The word we select by double clicking should remain selected regardless of
// where we drag the mouse afterwards without releasing the left button.
TEST_F(TextfieldTest, KeepInitiallySelectedWord) {
  InitTextfield();

  textfield_->SetText(ASCIIToUTF16("abc def ghi"));

  textfield_->SelectRange(gfx::Range(5, 5));
  const gfx::Rect middle_cursor = GetCursorBounds();
  textfield_->SelectRange(gfx::Range(0, 0));
  const gfx::Point beginning = GetCursorBounds().origin();

  // Double click, but do not release the left button.
  MouseClick(middle_cursor, 0);
  const gfx::Point middle(middle_cursor.x(),
                          middle_cursor.y() + middle_cursor.height() / 2);
  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, middle, middle,
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMousePressed(press_event);
  EXPECT_EQ(gfx::Range(4, 7), textfield_->GetSelectedRange());

  // Drag the mouse to the beginning of the textfield.
  ui::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, beginning, beginning,
                            ui::EF_LEFT_MOUSE_BUTTON, 0);
  textfield_->OnMouseDragged(drag_event);
  EXPECT_EQ(gfx::Range(7, 0), textfield_->GetSelectedRange());
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
TEST_F(TextfieldTest, SelectionClipboard) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("0123"));
  gfx::Point point_1(GetCursorPositionX(1), 0);
  gfx::Point point_2(GetCursorPositionX(2), 0);
  gfx::Point point_3(GetCursorPositionX(3), 0);
  gfx::Point point_4(GetCursorPositionX(4), 0);

  // Text selected by the mouse should be placed on the selection clipboard.
  ui::MouseEvent press(ui::ET_MOUSE_PRESSED, point_1, point_1,
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMousePressed(press);
  ui::MouseEvent drag(ui::ET_MOUSE_DRAGGED, point_3, point_3,
                      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMouseDragged(drag);
  ui::MouseEvent release(ui::ET_MOUSE_RELEASED, point_3, point_3,
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMouseReleased(release);
  EXPECT_EQ(gfx::Range(1, 3), textfield_->GetSelectedRange());
  EXPECT_STR_EQ("12", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));

  // Select-all should update the selection clipboard.
  SendKeyEvent(ui::VKEY_A, false, true);
  EXPECT_EQ(gfx::Range(0, 4), textfield_->GetSelectedRange());
  EXPECT_STR_EQ("0123", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_SELECTION, GetAndResetCopiedToClipboard());

  // Shift-click selection modifications should update the clipboard.
  NonClientMouseClick();
  ui::MouseEvent press_2(ui::ET_MOUSE_PRESSED, point_2, point_2,
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  press_2.set_flags(press_2.flags() | ui::EF_SHIFT_DOWN);
  textfield_->OnMousePressed(press_2);
  ui::MouseEvent release_2(ui::ET_MOUSE_RELEASED, point_2, point_2,
                           ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMouseReleased(release_2);
  EXPECT_EQ(gfx::Range(0, 2), textfield_->GetSelectedRange());
  EXPECT_STR_EQ("01", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_SELECTION, GetAndResetCopiedToClipboard());

  // Shift-Left/Right should update the selection clipboard.
  SendKeyEvent(ui::VKEY_RIGHT, true, false);
  EXPECT_STR_EQ("012", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_SELECTION, GetAndResetCopiedToClipboard());
  SendKeyEvent(ui::VKEY_LEFT, true, false);
  EXPECT_STR_EQ("01", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_SELECTION, GetAndResetCopiedToClipboard());
  SendKeyEvent(ui::VKEY_RIGHT, true, true);
  EXPECT_STR_EQ("0123", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_SELECTION, GetAndResetCopiedToClipboard());

  // Moving the cursor without a selection should not change the clipboard.
  SendKeyEvent(ui::VKEY_LEFT, false, false);
  EXPECT_EQ(gfx::Range(0, 0), textfield_->GetSelectedRange());
  EXPECT_STR_EQ("0123", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_LAST, GetAndResetCopiedToClipboard());

  // Middle clicking should paste at the mouse (not cursor) location.
  ui::MouseEvent middle(ui::ET_MOUSE_PRESSED, point_4, point_4,
                        ui::EF_MIDDLE_MOUSE_BUTTON, ui::EF_MIDDLE_MOUSE_BUTTON);
  textfield_->OnMousePressed(middle);
  EXPECT_STR_EQ("01230123", textfield_->text());
  EXPECT_EQ(gfx::Range(0, 0), textfield_->GetSelectedRange());
  EXPECT_STR_EQ("0123", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));

  // Middle click pasting should adjust trailing cursors.
  textfield_->SelectRange(gfx::Range(5, 5));
  textfield_->OnMousePressed(middle);
  EXPECT_STR_EQ("012301230123", textfield_->text());
  EXPECT_EQ(gfx::Range(9, 9), textfield_->GetSelectedRange());

  // Middle click pasting should adjust trailing selections.
  textfield_->SelectRange(gfx::Range(7, 9));
  textfield_->OnMousePressed(middle);
  EXPECT_STR_EQ("0123012301230123", textfield_->text());
  EXPECT_EQ(gfx::Range(11, 13), textfield_->GetSelectedRange());

  // Middle clicking in the selection should clear the clipboard and selection.
  textfield_->SelectRange(gfx::Range(2, 6));
  textfield_->OnMousePressed(middle);
  EXPECT_STR_EQ("0123012301230123", textfield_->text());
  EXPECT_EQ(gfx::Range(6, 6), textfield_->GetSelectedRange());
  EXPECT_TRUE(GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION).empty());

  // Double and triple clicking should update the clipboard contents.
  textfield_->SetText(ASCIIToUTF16("ab cd ef"));
  gfx::Point word(GetCursorPositionX(4), 0);
  ui::MouseEvent press_word(ui::ET_MOUSE_PRESSED, word, word,
                            ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMousePressed(press_word);
  ui::MouseEvent release_word(ui::ET_MOUSE_RELEASED, word, word,
                              ui::EF_LEFT_MOUSE_BUTTON,
                              ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMouseReleased(release_word);
  ui::MouseEvent double_click(ui::ET_MOUSE_PRESSED, word, word,
                              ui::EF_LEFT_MOUSE_BUTTON | ui::EF_IS_DOUBLE_CLICK,
                              ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMousePressed(double_click);
  textfield_->OnMouseReleased(release_word);
  EXPECT_EQ(gfx::Range(3, 5), textfield_->GetSelectedRange());
  EXPECT_STR_EQ("cd", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_SELECTION, GetAndResetCopiedToClipboard());
  textfield_->OnMousePressed(press_word);
  textfield_->OnMouseReleased(release_word);
  EXPECT_EQ(gfx::Range(0, 8), textfield_->GetSelectedRange());
  EXPECT_STR_EQ("ab cd ef", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_SELECTION, GetAndResetCopiedToClipboard());

  // Selecting a range of text without any user interaction should not change
  // the clipboard content.
  textfield_->SelectRange(gfx::Range(0, 3));
  EXPECT_STR_EQ("ab ", textfield_->GetSelectedText());
  EXPECT_STR_EQ("ab cd ef", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_LAST, GetAndResetCopiedToClipboard());

  SetClipboardText(ui::CLIPBOARD_TYPE_SELECTION, "other");
  textfield_->SelectAll(false);
  EXPECT_STR_EQ("other", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_LAST, GetAndResetCopiedToClipboard());
}
#endif

// Touch selection and dragging currently only works for chromeos.
#if defined(OS_CHROMEOS)
TEST_F(TextfieldTest, TouchSelectionAndDraggingTest) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));
  EXPECT_FALSE(test_api_->touch_selection_controller());
  const int x = GetCursorPositionX(2);
  GestureEventForTest tap(ui::ET_GESTURE_TAP, x, 0, 1.0f, 0.0f);
  GestureEventForTest tap_down(ui::ET_GESTURE_TAP_DOWN, x, 0, 0.0f, 0.0f);
  GestureEventForTest long_press(ui::ET_GESTURE_LONG_PRESS, x, 0, 0.0f, 0.0f);
  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableTouchEditing);

  // Tapping on the textfield should turn on the TouchSelectionController.
  textfield_->OnGestureEvent(&tap);
  EXPECT_TRUE(test_api_->touch_selection_controller());

  // Un-focusing the textfield should reset the TouchSelectionController
  textfield_->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(test_api_->touch_selection_controller());

  // With touch editing enabled, long press should not show context menu.
  // Instead, select word and invoke TouchSelectionController.
  textfield_->OnGestureEvent(&tap_down);
  textfield_->OnGestureEvent(&long_press);
  EXPECT_STR_EQ("hello", textfield_->GetSelectedText());
  EXPECT_TRUE(test_api_->touch_selection_controller());

  // With touch drag drop enabled, long pressing in the selected region should
  // start a drag and remove TouchSelectionController.
  ASSERT_TRUE(switches::IsTouchDragDropEnabled());
  textfield_->OnGestureEvent(&tap_down);
  textfield_->OnGestureEvent(&long_press);
  EXPECT_STR_EQ("hello", textfield_->GetSelectedText());
  EXPECT_FALSE(test_api_->touch_selection_controller());

  // After disabling touch drag drop, long pressing again in the selection
  // region should not do anything.
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableTouchDragDrop);
  ASSERT_FALSE(switches::IsTouchDragDropEnabled());
  textfield_->OnGestureEvent(&tap_down);
  textfield_->OnGestureEvent(&long_press);
  EXPECT_STR_EQ("hello", textfield_->GetSelectedText());
  EXPECT_TRUE(test_api_->touch_selection_controller());
  EXPECT_TRUE(long_press.handled());
}

TEST_F(TextfieldTest, TouchScrubbingSelection) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));
  EXPECT_FALSE(test_api_->touch_selection_controller());

  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableTouchEditing);

  // Simulate touch-scrubbing.
  int scrubbing_start = GetCursorPositionX(1);
  int scrubbing_end = GetCursorPositionX(6);

  GestureEventForTest tap_down(ui::ET_GESTURE_TAP_DOWN, scrubbing_start, 0,
                               0.0f, 0.0f);
  textfield_->OnGestureEvent(&tap_down);

  GestureEventForTest tap_cancel(ui::ET_GESTURE_TAP_CANCEL, scrubbing_start, 0,
                                 0.0f, 0.0f);
  textfield_->OnGestureEvent(&tap_cancel);

  GestureEventForTest scroll_begin(ui::ET_GESTURE_SCROLL_BEGIN, scrubbing_start,
                                   0, 0.0f, 0.0f);
  textfield_->OnGestureEvent(&scroll_begin);

  GestureEventForTest scroll_update(ui::ET_GESTURE_SCROLL_UPDATE, scrubbing_end,
                                    0, scrubbing_end - scrubbing_start, 0.0f);
  textfield_->OnGestureEvent(&scroll_update);

  GestureEventForTest scroll_end(ui::ET_GESTURE_SCROLL_END, scrubbing_end, 0,
                                 0.0f, 0.0f);
  textfield_->OnGestureEvent(&scroll_end);

  GestureEventForTest end(ui::ET_GESTURE_END, scrubbing_end, 0, 0.0f, 0.0f);
  textfield_->OnGestureEvent(&end);

  // In the end, part of text should have been selected and handles should have
  // appeared.
  EXPECT_STR_EQ("ello ", textfield_->GetSelectedText());
  EXPECT_TRUE(test_api_->touch_selection_controller());
}
#endif

// Long_Press gesture in Textfield can initiate a drag and drop now.
TEST_F(TextfieldTest, TestLongPressInitiatesDragDrop) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("Hello string world"));

  // Ensure the textfield will provide selected text for drag data.
  textfield_->SelectRange(gfx::Range(6, 12));
  const gfx::Point kStringPoint(GetCursorPositionX(9), 0);

  // Enable touch-drag-drop to make long press effective.
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableTouchDragDrop);

  // Create a long press event in the selected region should start a drag.
  GestureEventForTest long_press(ui::ET_GESTURE_LONG_PRESS, kStringPoint.x(),
                                 kStringPoint.y(), 0.0f, 0.0f);
  textfield_->OnGestureEvent(&long_press);
  EXPECT_TRUE(textfield_->CanStartDragForView(NULL, kStringPoint,
                                              kStringPoint));
}

TEST_F(TextfieldTest, GetTextfieldBaseline_FontFallbackTest) {
  InitTextfield();
  textfield_->SetText(UTF8ToUTF16("abc"));
  const int old_baseline = textfield_->GetBaseline();

  // Set text which may fall back to a font which has taller baseline than
  // the default font.
  textfield_->SetText(UTF8ToUTF16("\xE0\xB9\x91"));
  const int new_baseline = textfield_->GetBaseline();

  // Regardless of the text, the baseline must be the same.
  EXPECT_EQ(new_baseline, old_baseline);
}

}  // namespace views
